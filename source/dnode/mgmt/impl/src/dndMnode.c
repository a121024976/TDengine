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
 * along with this program. If not, see <http:www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "dndMnode.h"
#include "dndDnode.h"
#include "dndTransport.h"
#include "dndWorker.h"

static void dndProcessMnodeQueue(SDnode *pDnode, SMnodeMsg *pMsg);

static SMnode *dndAcquireMnode(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  SMnode     *pMnode = NULL;
  int32_t     refCount = 0;

  taosRLockLatch(&pMgmt->latch);
  if (pMgmt->deployed && !pMgmt->dropped) {
    refCount = atomic_add_fetch_32(&pMgmt->refCount, 1);
    pMnode = pMgmt->pMnode;
  } else {
    terrno = TSDB_CODE_DND_MNODE_NOT_DEPLOYED;
  }
  taosRUnLockLatch(&pMgmt->latch);

  if (pMnode != NULL) {
    dTrace("acquire mnode, refCount:%d", refCount);
  }
  return pMnode;
}

static void dndReleaseMnode(SDnode *pDnode, SMnode *pMnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  int32_t     refCount = 0;

  taosRLockLatch(&pMgmt->latch);
  if (pMnode != NULL) {
    refCount = atomic_sub_fetch_32(&pMgmt->refCount, 1);
  }
  taosRUnLockLatch(&pMgmt->latch);

  if (pMnode != NULL) {
    dTrace("release mnode, refCount:%d", refCount);
  }
}

static int32_t dndReadMnodeFile(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  int32_t     code = TSDB_CODE_DND_MNODE_READ_FILE_ERROR;
  int32_t     len = 0;
  int32_t     maxLen = 4096;
  char       *content = calloc(1, maxLen + 1);
  cJSON      *root = NULL;

  char file[PATH_MAX + 20];
  snprintf(file, PATH_MAX + 20, "%s/mnode.json", pDnode->dir.dnode);

  FILE *fp = fopen(file, "r");
  if (fp == NULL) {
    dDebug("file %s not exist", file);
    code = 0;
    goto PRASE_MNODE_OVER;
  }

  len = (int32_t)fread(content, 1, maxLen, fp);
  if (len <= 0) {
    dError("failed to read %s since content is null", file);
    goto PRASE_MNODE_OVER;
  }

  content[len] = 0;
  root = cJSON_Parse(content);
  if (root == NULL) {
    dError("failed to read %s since invalid json format", file);
    goto PRASE_MNODE_OVER;
  }

  cJSON *deployed = cJSON_GetObjectItem(root, "deployed");
  if (!deployed || deployed->type != cJSON_Number) {
    dError("failed to read %s since deployed not found", file);
    goto PRASE_MNODE_OVER;
  }
  pMgmt->deployed = deployed->valueint;

  cJSON *dropped = cJSON_GetObjectItem(root, "dropped");
  if (!dropped || dropped->type != cJSON_Number) {
    dError("failed to read %s since dropped not found", file);
    goto PRASE_MNODE_OVER;
  }
  pMgmt->dropped = dropped->valueint;

  cJSON *mnodes = cJSON_GetObjectItem(root, "mnodes");
  if (!mnodes || mnodes->type != cJSON_Array) {
    dError("failed to read %s since nodes not found", file);
    goto PRASE_MNODE_OVER;
  }

  pMgmt->replica = cJSON_GetArraySize(mnodes);
  if (pMgmt->replica <= 0 || pMgmt->replica > TSDB_MAX_REPLICA) {
    dError("failed to read %s since mnodes size %d invalid", file, pMgmt->replica);
    goto PRASE_MNODE_OVER;
  }

  for (int32_t i = 0; i < pMgmt->replica; ++i) {
    cJSON *node = cJSON_GetArrayItem(mnodes, i);
    if (node == NULL) break;

    SReplica *pReplica = &pMgmt->replicas[i];

    cJSON *id = cJSON_GetObjectItem(node, "id");
    if (!id || id->type != cJSON_Number) {
      dError("failed to read %s since id not found", file);
      goto PRASE_MNODE_OVER;
    }
    pReplica->id = id->valueint;

    cJSON *fqdn = cJSON_GetObjectItem(node, "fqdn");
    if (!fqdn || fqdn->type != cJSON_String || fqdn->valuestring == NULL) {
      dError("failed to read %s since fqdn not found", file);
      goto PRASE_MNODE_OVER;
    }
    tstrncpy(pReplica->fqdn, fqdn->valuestring, TSDB_FQDN_LEN);

    cJSON *port = cJSON_GetObjectItem(node, "port");
    if (!port || port->type != cJSON_Number) {
      dError("failed to read %s since port not found", file);
      goto PRASE_MNODE_OVER;
    }
    pReplica->port = port->valueint;
  }

  code = 0;
  dDebug("succcessed to read file %s, deployed:%d dropped:%d", file, pMgmt->deployed, pMgmt->dropped);

PRASE_MNODE_OVER:
  if (content != NULL) free(content);
  if (root != NULL) cJSON_Delete(root);
  if (fp != NULL) fclose(fp);

  terrno = code;
  return code;
}

static int32_t dndWriteMnodeFile(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  char file[PATH_MAX + 20];
  snprintf(file, PATH_MAX + 20, "%s/mnode.json.bak", pDnode->dir.dnode);

  FILE *fp = fopen(file, "w");
  if (fp == NULL) {
    terrno = TSDB_CODE_DND_MNODE_WRITE_FILE_ERROR;
    dError("failed to write %s since %s", file, terrstr());
    return -1;
  }

  int32_t len = 0;
  int32_t maxLen = 4096;
  char   *content = calloc(1, maxLen + 1);

  len += snprintf(content + len, maxLen - len, "{\n");
  len += snprintf(content + len, maxLen - len, "  \"deployed\": %d,\n", pMgmt->deployed);

  len += snprintf(content + len, maxLen - len, "  \"dropped\": %d,\n", pMgmt->dropped);
  len += snprintf(content + len, maxLen - len, "  \"mnodes\": [{\n");
  for (int32_t i = 0; i < pMgmt->replica; ++i) {
    SReplica *pReplica = &pMgmt->replicas[i];
    len += snprintf(content + len, maxLen - len, "    \"id\": %d,\n", pReplica->id);
    len += snprintf(content + len, maxLen - len, "    \"fqdn\": \"%s\",\n", pReplica->fqdn);
    len += snprintf(content + len, maxLen - len, "    \"port\": %u\n", pReplica->port);
    if (i < pMgmt->replica - 1) {
      len += snprintf(content + len, maxLen - len, "  },{\n");
    } else {
      len += snprintf(content + len, maxLen - len, "  }]\n");
    }
  }
  len += snprintf(content + len, maxLen - len, "}\n");

  fwrite(content, 1, len, fp);
  taosFsyncFile(fileno(fp));
  fclose(fp);
  free(content);

  char realfile[PATH_MAX + 20];
  snprintf(realfile, PATH_MAX + 20, "%s/mnode.json", pDnode->dir.dnode);

  if (taosRenameFile(file, realfile) != 0) {
    terrno = TSDB_CODE_DND_MNODE_WRITE_FILE_ERROR;
    dError("failed to rename %s since %s", file, terrstr());
    return -1;
  }

  dInfo("successed to write %s, deployed:%d dropped:%d", realfile, pMgmt->deployed, pMgmt->dropped);
  return 0;
}

static int32_t dndStartMnodeWorker(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  if (dndInitWorker(pDnode, &pMgmt->readWorker, DND_WORKER_SINGLE, "mnode-read", 0, 1, dndProcessMnodeQueue) != 0) {
    dError("failed to start mnode read worker since %s", terrstr());
    return -1;
  }

  if (dndInitWorker(pDnode, &pMgmt->writeWorker, DND_WORKER_SINGLE, "mnode-write", 0, 1, dndProcessMnodeQueue) != 0) {
    dError("failed to start mnode write worker since %s", terrstr());
    return -1;
  }

  if (dndInitWorker(pDnode, &pMgmt->syncWorker, DND_WORKER_SINGLE, "mnode-sync", 0, 1, dndProcessMnodeQueue) != 0) {
    dError("failed to start mnode sync worker since %s", terrstr());
    return -1;
  }

  return 0;
}

static void dndStopMnodeWorker(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  taosWLockLatch(&pMgmt->latch);
  pMgmt->deployed = 0;
  taosWUnLockLatch(&pMgmt->latch);

  while (pMgmt->refCount > 1) {
    taosMsleep(10);
  }

  dndCleanupWorker(&pMgmt->readWorker);
  dndCleanupWorker(&pMgmt->writeWorker);
  dndCleanupWorker(&pMgmt->syncWorker);
}

static bool dndNeedDeployMnode(SDnode *pDnode) {
  if (dndGetDnodeId(pDnode) > 0) {
    return false;
  }

  if (dndGetClusterId(pDnode) > 0) {
    return false;
  }

  if (strcmp(pDnode->opt.localEp, pDnode->opt.firstEp) != 0) {
    return false;
  }

  return true;
}

static void dndInitMnodeOption(SDnode *pDnode, SMnodeOpt *pOption) {
  pOption->pDnode = pDnode;
  pOption->sendMsgToDnodeFp = dndSendMsgToDnode;
  pOption->sendMsgToMnodeFp = dndSendMsgToMnode;
  pOption->sendRedirectMsgFp = dndSendRedirectMsg;
  pOption->dnodeId = dndGetDnodeId(pDnode);
  pOption->clusterId = dndGetClusterId(pDnode);
  pOption->cfg.sver = pDnode->opt.sver;
  pOption->cfg.enableTelem = pDnode->opt.enableTelem;
  pOption->cfg.statusInterval = pDnode->opt.statusInterval;
  pOption->cfg.shellActivityTimer = pDnode->opt.shellActivityTimer;
  pOption->cfg.timezone = pDnode->opt.timezone;
  pOption->cfg.charset = pDnode->opt.charset;
  pOption->cfg.locale = pDnode->opt.locale;
  pOption->cfg.gitinfo = pDnode->opt.gitinfo;
  pOption->cfg.buildinfo = pDnode->opt.buildinfo;
}

static void dndBuildMnodeDeployOption(SDnode *pDnode, SMnodeOpt *pOption) {
  dndInitMnodeOption(pDnode, pOption);
  pOption->replica = 1;
  pOption->selfIndex = 0;
  SReplica *pReplica = &pOption->replicas[0];
  pReplica->id = 1;
  pReplica->port = pDnode->opt.serverPort;
  memcpy(pReplica->fqdn, pDnode->opt.localFqdn, TSDB_FQDN_LEN);

  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  pMgmt->selfIndex = pOption->selfIndex;
  pMgmt->replica = pOption->replica;
  memcpy(&pMgmt->replicas, pOption->replicas, sizeof(SReplica) * TSDB_MAX_REPLICA);
}

static void dndBuildMnodeOpenOption(SDnode *pDnode, SMnodeOpt *pOption) {
  dndInitMnodeOption(pDnode, pOption);
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  pOption->selfIndex = pMgmt->selfIndex;
  pOption->replica = pMgmt->replica;
  memcpy(&pOption->replicas, pMgmt->replicas, sizeof(SReplica) * TSDB_MAX_REPLICA);
}

static int32_t dndBuildMnodeOptionFromMsg(SDnode *pDnode, SMnodeOpt *pOption, SDCreateMnodeMsg *pMsg) {
  dndInitMnodeOption(pDnode, pOption);
  pOption->dnodeId = dndGetDnodeId(pDnode);
  pOption->clusterId = dndGetClusterId(pDnode);

  pOption->replica = pMsg->replica;
  pOption->selfIndex = -1;
  for (int32_t i = 0; i < pMsg->replica; ++i) {
    SReplica *pReplica = &pOption->replicas[i];
    pReplica->id = pMsg->replicas[i].id;
    pReplica->port = pMsg->replicas[i].port;
    memcpy(pReplica->fqdn, pMsg->replicas[i].fqdn, TSDB_FQDN_LEN);
    if (pReplica->id == pOption->dnodeId) {
      pOption->selfIndex = i;
    }
  }

  if (pOption->selfIndex == -1) {
    terrno = TSDB_CODE_DND_MNODE_ID_NOT_FOUND;
    dError("failed to build mnode options since %s", terrstr());
    return -1;
  }

  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  pMgmt->selfIndex = pOption->selfIndex;
  pMgmt->replica = pOption->replica;
  memcpy(&pMgmt->replicas, pOption->replicas, sizeof(SReplica) * TSDB_MAX_REPLICA);
  return 0;
}

static int32_t dndOpenMnode(SDnode *pDnode, SMnodeOpt *pOption) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  SMnode *pMnode = mndOpen(pDnode->dir.mnode, pOption);
  if (pMnode == NULL) {
    dError("failed to open mnode since %s", terrstr());
    return -1;
  }

  if (dndStartMnodeWorker(pDnode) != 0) {
    dError("failed to start mnode worker since %s", terrstr());
    mndClose(pMnode);
    mndDestroy(pDnode->dir.mnode);
    return -1;
  }

  pMgmt->deployed = 1;
  if (dndWriteMnodeFile(pDnode) != 0) {
    dError("failed to write mnode file since %s", terrstr());
    pMgmt->deployed = 0;
    dndStopMnodeWorker(pDnode);
    mndClose(pMnode);
    mndDestroy(pDnode->dir.mnode);
    return -1;
  }

  taosWLockLatch(&pMgmt->latch);
  pMgmt->pMnode = pMnode;
  pMgmt->deployed = 1;
  taosWUnLockLatch(&pMgmt->latch);

  dInfo("mnode open successfully");
  return 0;
}

static int32_t dndAlterMnode(SDnode *pDnode, SMnodeOpt *pOption) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  SMnode *pMnode = dndAcquireMnode(pDnode);
  if (pMnode == NULL) {
    dError("failed to alter mnode since %s", terrstr());
    return -1;
  }

  if (mndAlter(pMnode, pOption) != 0) {
    dError("failed to alter mnode since %s", terrstr());
    dndReleaseMnode(pDnode, pMnode);
    return -1;
  }

  dndReleaseMnode(pDnode, pMnode);
  return 0;
}

static int32_t dndDropMnode(SDnode *pDnode) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  SMnode *pMnode = dndAcquireMnode(pDnode);
  if (pMnode == NULL) {
    dError("failed to drop mnode since %s", terrstr());
    return -1;
  }

  taosRLockLatch(&pMgmt->latch);
  pMgmt->dropped = 1;
  taosRUnLockLatch(&pMgmt->latch);

  if (dndWriteMnodeFile(pDnode) != 0) {
    taosRLockLatch(&pMgmt->latch);
    pMgmt->dropped = 0;
    taosRUnLockLatch(&pMgmt->latch);

    dndReleaseMnode(pDnode, pMnode);
    dError("failed to drop mnode since %s", terrstr());
    return -1;
  }

  dndReleaseMnode(pDnode, pMnode);
  dndStopMnodeWorker(pDnode);
  pMgmt->deployed = 0;
  dndWriteMnodeFile(pDnode);
  mndClose(pMnode);
  pMgmt->pMnode = NULL;
  mndDestroy(pDnode->dir.mnode);

  return 0;
}

static SDCreateMnodeMsg *dndParseCreateMnodeMsg(SRpcMsg *pRpcMsg) {
  SDCreateMnodeMsg *pMsg = pRpcMsg->pCont;
  pMsg->dnodeId = htonl(pMsg->dnodeId);
  for (int32_t i = 0; i < pMsg->replica; ++i) {
    pMsg->replicas[i].id = htonl(pMsg->replicas[i].id);
    pMsg->replicas[i].port = htons(pMsg->replicas[i].port);
  }

  return pMsg;
}

int32_t dndProcessCreateMnodeReq(SDnode *pDnode, SRpcMsg *pRpcMsg) {
  SDCreateMnodeMsg *pMsg = dndParseCreateMnodeMsg(pRpcMsg);

  if (pMsg->dnodeId != dndGetDnodeId(pDnode)) {
    terrno = TSDB_CODE_DND_MNODE_ID_INVALID;
    return -1;
  } else {
    SMnodeOpt option = {0};
    if (dndBuildMnodeOptionFromMsg(pDnode, &option, pMsg) != 0) {
      return -1;
    }

    return dndOpenMnode(pDnode, &option);
  }
}

int32_t dndProcessAlterMnodeReq(SDnode *pDnode, SRpcMsg *pRpcMsg) {
  SDAlterMnodeMsg *pMsg = dndParseCreateMnodeMsg(pRpcMsg);

  if (pMsg->dnodeId != dndGetDnodeId(pDnode)) {
    terrno = TSDB_CODE_DND_MNODE_ID_INVALID;
    return -1;
  }

  SMnodeOpt option = {0};
  if (dndBuildMnodeOptionFromMsg(pDnode, &option, pMsg) != 0) {
    return -1;
  }

  if (dndAlterMnode(pDnode, &option) != 0) {
    return -1;
  }

  return dndWriteMnodeFile(pDnode);
}

int32_t dndProcessDropMnodeReq(SDnode *pDnode, SRpcMsg *pRpcMsg) {
  SDDropMnodeMsg *pMsg = pRpcMsg->pCont;
  pMsg->dnodeId = htonl(pMsg->dnodeId);

  if (pMsg->dnodeId != dndGetDnodeId(pDnode)) {
    terrno = TSDB_CODE_DND_MNODE_ID_INVALID;
    return -1;
  } else {
    return dndDropMnode(pDnode);
  }
}

static void dndProcessMnodeQueue(SDnode *pDnode, SMnodeMsg *pMsg) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  SMnode *pMnode = dndAcquireMnode(pDnode);
  if (pMnode != NULL) {
    mndProcessMsg(pMsg);
    dndReleaseMnode(pDnode, pMnode);
  } else {
    mndSendRsp(pMsg, terrno);
  }

  mndCleanupMsg(pMsg);
}

static void dndWriteMnodeMsgToWorker(SDnode *pDnode, SDnodeWorker *pWorker, SRpcMsg *pRpcMsg) {
  int32_t code = TSDB_CODE_DND_MNODE_NOT_DEPLOYED;

  SMnode *pMnode = dndAcquireMnode(pDnode);
  if (pMnode != NULL) {
    SMnodeMsg *pMsg = mndInitMsg(pMnode, pRpcMsg);
    if (pMsg == NULL) {
      code = TSDB_CODE_OUT_OF_MEMORY;
    } else {
      code = dndWriteMsgToWorker(pWorker, pMsg, 0);
    }

    if (code != 0) {
      mndCleanupMsg(pMsg);
    }
  }
  dndReleaseMnode(pDnode, pMnode);

  if (code != 0) {
    if (pRpcMsg->msgType & 1u) {
      SRpcMsg rsp = {.handle = pRpcMsg->handle, .ahandle = pRpcMsg->ahandle, .code = code};
      rpcSendResponse(&rsp);
    }
    rpcFreeCont(pRpcMsg->pCont);
  }
}

void dndProcessMnodeWriteMsg(SDnode *pDnode, SRpcMsg *pMsg, SEpSet *pEpSet) {
  dndWriteMnodeMsgToWorker(pDnode, &pDnode->mmgmt.writeWorker, pMsg);
}

void dndProcessMnodeSyncMsg(SDnode *pDnode, SRpcMsg *pMsg, SEpSet *pEpSet) {
  dndWriteMnodeMsgToWorker(pDnode, &pDnode->mmgmt.syncWorker, pMsg);
}

void dndProcessMnodeReadMsg(SDnode *pDnode, SRpcMsg *pMsg, SEpSet *pEpSet) {
  dndWriteMnodeMsgToWorker(pDnode, &pDnode->mmgmt.readWorker, pMsg);
}

int32_t dndInitMnode(SDnode *pDnode) {
  dInfo("dnode-mnode start to init");
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  taosInitRWLatch(&pMgmt->latch);

  if (dndReadMnodeFile(pDnode) != 0) {
    return -1;
  }

  if (pMgmt->dropped) {
    dInfo("mnode has been deployed and needs to be deleted");
    mndDestroy(pDnode->dir.mnode);
    return 0;
  }

  if (!pMgmt->deployed) {
    bool needDeploy = dndNeedDeployMnode(pDnode);
    if (!needDeploy) {
      dDebug("mnode does not need to be deployed");
      return 0;
    }

    dInfo("start to deploy mnode");
    SMnodeOpt option = {0};
    dndBuildMnodeDeployOption(pDnode, &option);
    return dndOpenMnode(pDnode, &option);
  } else {
    dInfo("start to open mnode");
    SMnodeOpt option = {0};
    dndBuildMnodeOpenOption(pDnode, &option);
    return dndOpenMnode(pDnode, &option);
  }
}

void dndCleanupMnode(SDnode *pDnode) {
  dInfo("dnode-mnode start to clean up");
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;
  if (pMgmt->pMnode) {
    dndStopMnodeWorker(pDnode);
    mndClose(pMgmt->pMnode);
    pMgmt->pMnode = NULL;
  }
  dInfo("dnode-mnode is cleaned up");
}

int32_t dndGetUserAuthFromMnode(SDnode *pDnode, char *user, char *spi, char *encrypt, char *secret, char *ckey) {
  SMnodeMgmt *pMgmt = &pDnode->mmgmt;

  SMnode *pMnode = dndAcquireMnode(pDnode);
  if (pMnode == NULL) {
    terrno = TSDB_CODE_APP_NOT_READY;
    dTrace("failed to get user auth since %s", terrstr());
    return -1;
  }

  int32_t code = mndRetriveAuth(pMnode, user, spi, encrypt, secret, ckey);
  dndReleaseMnode(pDnode, pMnode);
  return code;
}