/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "mndConsumer.h"
#include "mndDb.h"
#include "mndDnode.h"
#include "mndMnode.h"
#include "mndShow.h"
#include "mndStb.h"
#include "mndTopic.h"
#include "mndTrans.h"
#include "mndUser.h"
#include "mndVgroup.h"
#include "tcompare.h"
#include "tname.h"

#define MND_CONSUMER_VER_NUMBER 1
#define MND_CONSUMER_RESERVE_SIZE 64

static SSdbRaw *mndConsumerActionEncode(SMqConsumerObj *pConsumer);
static SSdbRow *mndConsumerActionDecode(SSdbRaw *pRaw);
static int32_t  mndConsumerActionInsert(SSdb *pSdb, SMqConsumerObj *pConsumer);
static int32_t  mndConsumerActionDelete(SSdb *pSdb, SMqConsumerObj *pConsumer);
static int32_t  mndConsumerActionUpdate(SSdb *pSdb, SMqConsumerObj *pConsumer, SMqConsumerObj *pNewConsumer);
static int32_t  mndProcessCreateConsumerMsg(SMnodeMsg *pMsg);
static int32_t  mndProcessDropConsumerMsg(SMnodeMsg *pMsg);
static int32_t  mndProcessDropConsumerInRsp(SMnodeMsg *pMsg);
static int32_t  mndProcessConsumerMetaMsg(SMnodeMsg *pMsg);
static int32_t  mndGetConsumerMeta(SMnodeMsg *pMsg, SShowObj *pShow, STableMetaMsg *pMeta);
static int32_t  mndRetrieveConsumer(SMnodeMsg *pMsg, SShowObj *pShow, char *data, int32_t rows);
static void     mndCancelGetNextConsumer(SMnode *pMnode, void *pIter);

static int32_t mndProcessSubscribeReq(SMnodeMsg *pMsg);
static int32_t mndProcessSubscribeRsp(SMnodeMsg *pMsg);
static int32_t mndProcessSubscribeInternalReq(SMnodeMsg *pMsg);
static int32_t mndProcessSubscribeInternalRsp(SMnodeMsg *pMsg);

int32_t mndInitConsumer(SMnode *pMnode) {
  SSdbTable table = {.sdbType = SDB_CONSUMER,
                     .keyType = SDB_KEY_BINARY,
                     .encodeFp = (SdbEncodeFp)mndConsumerActionEncode,
                     .decodeFp = (SdbDecodeFp)mndConsumerActionDecode,
                     .insertFp = (SdbInsertFp)mndConsumerActionInsert,
                     .updateFp = (SdbUpdateFp)mndConsumerActionUpdate,
                     .deleteFp = (SdbDeleteFp)mndConsumerActionDelete};

  mndSetMsgHandle(pMnode, TDMT_MND_SUBSCRIBE, mndProcessSubscribeReq);
  /*mndSetMsgHandle(pMnode, TDMT_MND_SUBSCRIBE_RSP, mndProcessSubscribeRsp);*/
  /*mndSetMsgHandle(pMnode, TDMT_VND_SUBSCRIBE, mndProcessSubscribeInternalReq);*/
  mndSetMsgHandle(pMnode, TDMT_VND_SUBSCRIBE_RSP, mndProcessSubscribeInternalRsp);

  return sdbSetTable(pMnode->pSdb, table);
}

void mndCleanupConsumer(SMnode *pMnode) {}

static void *mndBuildMqVGroupSetReq(SMnode *pMnode, char *topicName, int32_t vgId, int64_t consumerId, char *cgroup) {
  return 0;
}

static SSdbRaw *mndConsumerActionEncode(SMqConsumerObj *pConsumer) {
  int32_t  size = sizeof(SMqConsumerObj) + MND_CONSUMER_RESERVE_SIZE;
  SSdbRaw *pRaw = sdbAllocRaw(SDB_CONSUMER, MND_CONSUMER_VER_NUMBER, size);
  if (pRaw == NULL) return NULL;

  int32_t dataPos = 0;
  int32_t topicNum = taosArrayGetSize(pConsumer->topics);
  SDB_SET_INT64(pRaw, dataPos, pConsumer->consumerId);
  int32_t len = strlen(pConsumer->cgroup);
  SDB_SET_INT32(pRaw, dataPos, len);
  SDB_SET_BINARY(pRaw, dataPos, pConsumer->cgroup, len);
  SDB_SET_INT32(pRaw, dataPos, topicNum);
  for (int i = 0; i < topicNum; i++) {
    int32_t           len;
    SMqConsumerTopic *pConsumerTopic = taosArrayGet(pConsumer->topics, i);
    len = strlen(pConsumerTopic->name);
    SDB_SET_INT32(pRaw, dataPos, len);
    SDB_SET_BINARY(pRaw, dataPos, pConsumerTopic->name, len);
    int vgSize;
    if (pConsumerTopic->vgroups == NULL) {
      vgSize = 0;
    } else {
      vgSize = listNEles(pConsumerTopic->vgroups);
    }
    SDB_SET_INT32(pRaw, dataPos, vgSize);
    for (int j = 0; j < vgSize; j++) {
      // SList* head;
      /*SDB_SET_INT64(pRaw, dataPos, 0[> change to list item <]);*/
    }
  }

  SDB_SET_RESERVE(pRaw, dataPos, MND_CONSUMER_RESERVE_SIZE);
  SDB_SET_DATALEN(pRaw, dataPos);

  return pRaw;
}

static SSdbRow *mndConsumerActionDecode(SSdbRaw *pRaw) {
  int8_t sver = 0;
  if (sdbGetRawSoftVer(pRaw, &sver) != 0) return NULL;

  if (sver != MND_CONSUMER_VER_NUMBER) {
    terrno = TSDB_CODE_SDB_INVALID_DATA_VER;
    mError("failed to decode consumer since %s", terrstr());
    return NULL;
  }

  int32_t         size = sizeof(SMqConsumerObj);
  SSdbRow        *pRow = sdbAllocRow(size);
  SMqConsumerObj *pConsumer = sdbGetRowObj(pRow);
  if (pConsumer == NULL) return NULL;

  int32_t dataPos = 0;
  SDB_GET_INT64(pRaw, pRow, dataPos, &pConsumer->consumerId);
  int32_t len, topicNum;
  SDB_GET_INT32(pRaw, pRow, dataPos, &len);
  SDB_GET_BINARY(pRaw, pRow, dataPos, pConsumer->cgroup, len);
  SDB_GET_INT32(pRaw, pRow, dataPos, &topicNum);
  for (int i = 0; i < topicNum; i++) {
    int32_t           topicLen;
    SMqConsumerTopic *pConsumerTopic = malloc(sizeof(SMqConsumerTopic));
    if (pConsumerTopic == NULL) {
      terrno = TSDB_CODE_OUT_OF_MEMORY;
      // TODO
      return NULL;
    }
    /*pConsumerTopic->vgroups = taosArrayInit(topicNum, sizeof(SMqConsumerTopic));*/
    SDB_GET_INT32(pRaw, pRow, dataPos, &topicLen);
    SDB_GET_BINARY(pRaw, pRow, dataPos, pConsumerTopic->name, topicLen);
    int32_t vgSize;
    SDB_GET_INT32(pRaw, pRow, dataPos, &vgSize);
  }

  SDB_GET_RESERVE(pRaw, pRow, dataPos, MND_CONSUMER_RESERVE_SIZE);

  return pRow;
}

static int32_t mndConsumerActionInsert(SSdb *pSdb, SMqConsumerObj *pConsumer) {
  mTrace("consumer:%ld, perform insert action", pConsumer->consumerId);
  return 0;
}

static int32_t mndConsumerActionDelete(SSdb *pSdb, SMqConsumerObj *pConsumer) {
  mTrace("consumer:%ld, perform delete action", pConsumer->consumerId);
  return 0;
}

static int32_t mndConsumerActionUpdate(SSdb *pSdb, SMqConsumerObj *pOldConsumer, SMqConsumerObj *pNewConsumer) {
  mTrace("consumer:%ld, perform update action", pOldConsumer->consumerId);

  // TODO handle update
  /*taosWLockLatch(&pOldConsumer->lock);*/
  /*taosWUnLockLatch(&pOldConsumer->lock);*/

  return 0;
}

SMqConsumerObj *mndAcquireConsumer(SMnode *pMnode, int32_t consumerId) {
  SSdb           *pSdb = pMnode->pSdb;
  SMqConsumerObj *pConsumer = sdbAcquire(pSdb, SDB_CONSUMER, &consumerId);
  if (pConsumer == NULL) {
    /*terrno = TSDB_CODE_MND_CONSUMER_NOT_EXIST;*/
  }
  return pConsumer;
}

void mndReleaseConsumer(SMnode *pMnode, SMqConsumerObj *pConsumer) {
  SSdb *pSdb = pMnode->pSdb;
  sdbRelease(pSdb, pConsumer);
}

static int32_t mndProcessSubscribeReq(SMnodeMsg *pMsg) {
  SMnode          *pMnode = pMsg->pMnode;
  char            *msgStr = pMsg->rpcMsg.pCont;
  SCMSubscribeReq *pSubscribe;
  tDeserializeSCMSubscribeReq(msgStr, pSubscribe);
  int64_t consumerId = pSubscribe->consumerId;
  char   *consumerGroup = pSubscribe->consumerGroup;

  SArray *newSub = NULL;
  int     newTopicNum = pSubscribe->topicNum;
  if (newTopicNum) {
    newSub = taosArrayInit(newTopicNum, sizeof(SMqConsumerTopic));
  }
  for (int i = 0; i < newTopicNum; i++) {
    char             *topic = pSubscribe->topicName[i];
    SMqConsumerTopic *pConsumerTopic = malloc(sizeof(SMqConsumerTopic));
    if (pConsumerTopic == NULL) {
      terrno = TSDB_CODE_OUT_OF_MEMORY;
      // TODO: free
      return -1;
    }
    pConsumerTopic->vgroups = tdListNew(sizeof(int64_t));
    taosArrayPush(newSub, pConsumerTopic);
    free(pConsumerTopic);
  }
  taosArraySortString(newSub, taosArrayCompareString);

  SArray         *oldSub = NULL;
  int             oldTopicNum = 0;
  SMqConsumerObj *pConsumer = mndAcquireConsumer(pMnode, consumerId);
  if (pConsumer == NULL) {
    // create consumer
    pConsumer = malloc(sizeof(SMqConsumerObj));
    if (pConsumer == NULL) {
      terrno = TSDB_CODE_OUT_OF_MEMORY;
      return -1;
    }
    strcpy(pConsumer->cgroup, pSubscribe->consumerGroup);

  } else {
    oldSub = pConsumer->topics;
    oldTopicNum = taosArrayGetSize(oldSub);
  }
  STrans *pTrans = mndTransCreate(pMnode, TRN_POLICY_RETRY, &pMsg->rpcMsg);
  if (pTrans == NULL) {
    return -1;
  }

  int i = 0, j = 0;
  while (i < newTopicNum || j < oldTopicNum) {
    SMqConsumerTopic *pOldTopic = NULL;
    SMqConsumerTopic *pNewTopic = NULL;
    if (i >= newTopicNum) {
      // encode unset topic msg to all vnodes related to that topic
      pOldTopic = taosArrayGet(oldSub, j);
      j++;
    } else if (j >= oldTopicNum) {
      pNewTopic = taosArrayGet(newSub, i);
    } else {
      pNewTopic = taosArrayGet(newSub, i);
      pOldTopic = taosArrayGet(oldSub, j);

      char *newName = pNewTopic->name;
      char *oldName = pOldTopic->name;
      int   comp = compareLenPrefixedStr(newName, oldName);
      if (comp == 0) {
        // do nothing
        pOldTopic = pNewTopic = NULL;
        i++;
        j++;
        continue;
      } else if (comp < 0) {
        pOldTopic = NULL;
        i++;
      } else {
        pNewTopic = NULL;
        j++;
      }
    }

    if (pOldTopic != NULL) {
      ASSERT(pNewTopic == NULL);
      char     *oldTopicName = pOldTopic->name;
      SList    *vgroups = pOldTopic->vgroups;
      SListIter iter;
      tdListInitIter(vgroups, &iter, TD_LIST_FORWARD);
      SListNode *pn;

      SMqTopicObj *pTopic = mndAcquireTopic(pMnode, oldTopicName);
      ASSERT(pTopic != NULL);
      SMqCGroup *pGroup = taosHashGet(pTopic->cgroups, pSubscribe->consumerGroup, strlen(pSubscribe->consumerGroup));
      while ((pn = tdListNext(&iter)) != NULL) {
        int32_t vgId = *(int64_t *)pn->data;
        SVgObj *pVgObj = mndAcquireVgroup(pMnode, vgId);
        // TODO release
        if (pVgObj == NULL) {
          // TODO handle error
          continue;
        }
        // acquire and get epset
        void *pMqVgSetReq =
            mndBuildMqVGroupSetReq(pMnode, oldTopicName, vgId, pSubscribe->consumerId, pSubscribe->consumerGroup);
        // TODO:serialize
        if (pMsg == NULL) {
          terrno = TSDB_CODE_OUT_OF_MEMORY;
          return -1;
        }
        STransAction action = {0};
        action.epSet = mndGetVgroupEpset(pMnode, pVgObj);
        action.pCont = pMqVgSetReq;
        action.contLen = 0;  // TODO
        action.msgType = TDMT_VND_MQ_SET_CONN;
        if (mndTransAppendRedoAction(pTrans, &action) != 0) {
          free(pMqVgSetReq);
          mndTransDrop(pTrans);
          // TODO free
          return -1;
        }
      }
      taosHashRemove(pTopic->cgroups, pSubscribe->consumerGroup, strlen(pSubscribe->consumerGroup));

    } else if (pNewTopic != NULL) {
      ASSERT(pOldTopic == NULL);

      char        *newTopicName = pNewTopic->name;
      SMqTopicObj *pTopic = mndAcquireTopic(pMnode, newTopicName);
      ASSERT(pTopic != NULL);

      SMqCGroup *pGroup = taosHashGet(pTopic->cgroups, pSubscribe->consumerGroup, strlen(pSubscribe->consumerGroup));
      if (pGroup == NULL) {
        // add new group
        pGroup = malloc(sizeof(SMqCGroup));
        if (pGroup == NULL) {
          terrno = TSDB_CODE_OUT_OF_MEMORY;
          return -1;
        }
        pGroup->consumerIds = tdListNew(sizeof(int64_t));
        if (pGroup->consumerIds == NULL) {
          free(pGroup);
          terrno = TSDB_CODE_OUT_OF_MEMORY;
          return -1;
        }
        pGroup->status = 0;
        // add into cgroups
        taosHashPut(pTopic->cgroups, pSubscribe->consumerGroup, strlen(pSubscribe->consumerGroup), pGroup,
                    sizeof(SMqCGroup));
      }

      // put the consumer into list
      // rebalance will be triggered by timer
      tdListAppend(pGroup->consumerIds, &pSubscribe->consumerId);

      SSdbRaw *pTopicRaw = mndTopicActionEncode(pTopic);
      sdbSetRawStatus(pTopicRaw, SDB_STATUS_READY);
      // TODO: error handling
      mndTransAppendRedolog(pTrans, pTopicRaw);
    } else {
      ASSERT(0);
    }
  }
  // destroy old sub
  taosArrayDestroy(oldSub);
  // put new sub into consumerobj
  pConsumer->topics = newSub;

  // persist consumerObj
  SSdbRaw *pConsumerRaw = mndConsumerActionEncode(pConsumer);
  sdbSetRawStatus(pConsumerRaw, SDB_STATUS_READY);
  // TODO: error handling
  mndTransAppendRedolog(pTrans, pConsumerRaw);

  if (mndTransPrepare(pMnode, pTrans) != 0) {
    mError("trans:%d, failed to prepare since %s", pTrans->id, terrstr());
    mndTransDrop(pTrans);
    return -1;
  }

  // TODO: free memory
  mndTransDrop(pTrans);
  return 0;
}

static int32_t mndProcessSubscribeInternalRsp(SMnodeMsg *pMsg) { return 0; }

static int32_t mndProcessConsumerMetaMsg(SMnodeMsg *pMsg) {
  SMnode        *pMnode = pMsg->pMnode;
  STableInfoMsg *pInfo = pMsg->rpcMsg.pCont;

  mDebug("consumer:%s, start to retrieve meta", pInfo->tableFname);

#if 0
  SDbObj *pDb = mndAcquireDbByConsumer(pMnode, pInfo->tableFname);
  if (pDb == NULL) {
    terrno = TSDB_CODE_MND_DB_NOT_SELECTED;
    mError("consumer:%s, failed to retrieve meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  SConsumerObj *pConsumer = mndAcquireConsumer(pMnode, pInfo->tableFname);
  if (pConsumer == NULL) {
    mndReleaseDb(pMnode, pDb);
    terrno = TSDB_CODE_MND_INVALID_CONSUMER;
    mError("consumer:%s, failed to get meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  taosRLockLatch(&pConsumer->lock);
  int32_t totalCols = pConsumer->numOfColumns + pConsumer->numOfTags;
  int32_t contLen = sizeof(STableMetaMsg) + totalCols * sizeof(SSchema);

  STableMetaMsg *pMeta = rpcMallocCont(contLen);
  if (pMeta == NULL) {
    taosRUnLockLatch(&pConsumer->lock);
    mndReleaseDb(pMnode, pDb);
    mndReleaseConsumer(pMnode, pConsumer);
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    mError("consumer:%s, failed to get meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  memcpy(pMeta->consumerFname, pConsumer->name, TSDB_TABLE_FNAME_LEN);
  pMeta->numOfTags = htonl(pConsumer->numOfTags);
  pMeta->numOfColumns = htonl(pConsumer->numOfColumns);
  pMeta->precision = pDb->cfg.precision;
  pMeta->tableType = TSDB_SUPER_TABLE;
  pMeta->update = pDb->cfg.update;
  pMeta->sversion = htonl(pConsumer->version);
  pMeta->tuid = htonl(pConsumer->uid);

  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pMeta->pSchema[i];
    SSchema *pSrcSchema = &pConsumer->pSchema[i];
    memcpy(pSchema->name, pSrcSchema->name, TSDB_COL_NAME_LEN);
    pSchema->type = pSrcSchema->type;
    pSchema->colId = htonl(pSrcSchema->colId);
    pSchema->bytes = htonl(pSrcSchema->bytes);
  }
  taosRUnLockLatch(&pConsumer->lock);
  mndReleaseDb(pMnode, pDb);
  mndReleaseConsumer(pMnode, pConsumer);

  pMsg->pCont = pMeta;
  pMsg->contLen = contLen;

  mDebug("consumer:%s, meta is retrieved, cols:%d tags:%d", pInfo->tableFname, pConsumer->numOfColumns, pConsumer->numOfTags);
#endif
  return 0;
}

static int32_t mndGetNumOfConsumers(SMnode *pMnode, char *dbName, int32_t *pNumOfConsumers) {
  SSdb *pSdb = pMnode->pSdb;

  SDbObj *pDb = mndAcquireDb(pMnode, dbName);
  if (pDb == NULL) {
    terrno = TSDB_CODE_MND_DB_NOT_SELECTED;
    return -1;
  }

  int32_t numOfConsumers = 0;
  void   *pIter = NULL;
  while (1) {
    SMqConsumerObj *pConsumer = NULL;
    pIter = sdbFetch(pSdb, SDB_CONSUMER, pIter, (void **)&pConsumer);
    if (pIter == NULL) break;

    numOfConsumers++;

    sdbRelease(pSdb, pConsumer);
  }

  *pNumOfConsumers = numOfConsumers;
  return 0;
}

static int32_t mndGetConsumerMeta(SMnodeMsg *pMsg, SShowObj *pShow, STableMetaMsg *pMeta) {
  SMnode *pMnode = pMsg->pMnode;
  SSdb   *pSdb = pMnode->pSdb;

  if (mndGetNumOfConsumers(pMnode, pShow->db, &pShow->numOfRows) != 0) {
    return -1;
  }

  int32_t  cols = 0;
  SSchema *pSchema = pMeta->pSchema;

  pShow->bytes[cols] = TSDB_TABLE_NAME_LEN + VARSTR_HEADER_SIZE;
  pSchema[cols].type = TSDB_DATA_TYPE_BINARY;
  strcpy(pSchema[cols].name, "name");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 8;
  pSchema[cols].type = TSDB_DATA_TYPE_TIMESTAMP;
  strcpy(pSchema[cols].name, "create_time");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 4;
  pSchema[cols].type = TSDB_DATA_TYPE_INT;
  strcpy(pSchema[cols].name, "columns");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 4;
  pSchema[cols].type = TSDB_DATA_TYPE_INT;
  strcpy(pSchema[cols].name, "tags");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pMeta->numOfColumns = htonl(cols);
  pShow->numOfColumns = cols;

  pShow->offset[0] = 0;
  for (int32_t i = 1; i < cols; ++i) {
    pShow->offset[i] = pShow->offset[i - 1] + pShow->bytes[i - 1];
  }

  pShow->numOfRows = sdbGetSize(pSdb, SDB_CONSUMER);
  pShow->rowSize = pShow->offset[cols - 1] + pShow->bytes[cols - 1];
  strcpy(pMeta->tbFname, mndShowStr(pShow->type));

  return 0;
}

static void mndCancelGetNextConsumer(SMnode *pMnode, void *pIter) {
  SSdb *pSdb = pMnode->pSdb;
  sdbCancelFetch(pSdb, pIter);
}
