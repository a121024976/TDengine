###################################################################
#           Copyright (c) 2016 by TAOS Technologies, Inc.
#                     All rights reserved.
#
#  This file is proprietary and confidential to TAOS Technologies.
#  No part of this file may be reproduced, stored, transmitted,
#  disclosed or used in any form or by any means other than as
#  expressly provided by the written permission from Jianhui Tao
#
###################################################################

# -*- coding: utf-8 -*-

import random
import string
import os
import sys
import time
import taos
from util.log import tdLog
from util.cases import tdCases
from util.sql import tdSql
from util.dnodes import tdDnodes

class TDTestCase:

    def run(self):
        os.system("cd /root/log/")
        os.system("sudo rm -rfv *cmd*txt")
        os.system("cd /root/gxy/TDinternal/community/tests/pytest")

        for i in range(1000):           
            try:
                testcmd1 = os.system("sudo python3 ./test.py -f query_new/auto_makesql.py   >>/root/log/testcmd1.txt")
                print ("The regular_query_null.py num:%d result is %d " % (i ,  testcmd1))
                # testcmd2 = os.system("sudo python3 ./test.py -f query_new/regular_query_union.py  >>/root/log/testcmd2.txt")                
                # print ("The regular_query_union.py num:%d result is %d " % (i ,  testcmd2))
                # testcmd3 = os.system("sudo python3 ./test.py -f query_new/regular_query.py  >>/root/log/testcmd3.txt")                
                # print ("The regular_query.py num:%d result is %d " % (i ,  testcmd2))

                # if testcmd1==0 and testcmd2==0 and testcmd3==0:
                #     continue
                # elif :
                #     continue
                if testcmd1==0 :
                    continue
                else:
                    break

            except Exception as e:
                print(e)
          
            
        # for i in range(4000):           
        #     try:
        #         testcmd6 = os.system("python3 ./test.py -f query/nestedQuery/nestedQuery1.py >>/home/ubuntu/log/testcmd6.txt")
        #         print ("The nestedQuery1.py num:%d result is %d " % (i , testcmd6))  
        #         if testcmd6==0:
        #             continue
        #         else:
        #             break
            
        #     except Exception as e:
        #         print(e)
                    



test1 = TDTestCase()
test1.run()
