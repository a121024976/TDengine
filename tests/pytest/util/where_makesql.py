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
from util.log import tdLog
from util.cases import tdCases
from util.sql import tdSql
from util.dnodes import *
import itertools
from itertools import product
from itertools import combinations
from faker import Faker

class TDWhere_makesql:
    updatecfgDict={'maxSQLLength':1048576}
    NUM = random.randint(5, 6)
    print(NUM)

    def init(self, conn, logSql):
        tdLog.debug("start to execute %s" % __file__)
        tdSql.init(conn.cursor(), logSql)       

    # column and tag query
    # **int + floot_dou + other
    # q_where + t_where | q_where_null + t_where_null [前2组union] | q_where_all + t_where_all[自己union]
    def q_where(self):       
        # q_where = ['q_binary match \'binary%\'  'q_binary like \'binary%\'  or q_nchar = \'0\' ']

        q_int_where = ['q_bigint >= -9223372036854775807 and ' , 'q_bigint <= 9223372036854775807 and ','q_smallint >= -32767 and ', 'q_smallint <= 32767 and ',
        'q_tinyint >= -127 and ' , 'q_tinyint <= 127 and ' , 'q_int <= 2147483647 and ' , 'q_int >= -2147483647 and ',
        'q_tinyint != 128 and ',
        'q_bigint between  -9223372036854775807 and 9223372036854775807 and ',' q_int between -2147483647 and 2147483647 and ',
        'q_smallint between -32767 and 32767 and ', 'q_tinyint between -127 and 127  and ',
        'q_bigint is not null and ' , 'q_int is not null and ' , 'q_smallint is not null and ' , 'q_tinyint is not null and ' ,]

        q_fl_do_where = ['q_float >= -3.4E38 and ','q_float <= 3.4E38 and ', 'q_double >= -1.7E308 and ','q_double <= 1.7E308 and ', 
        'q_float between -3.4E38 and 3.4E38 and ','q_double between -1.7E308 and 1.7E308 and ' ,
        'q_float is not null and ' ,'q_double is not null and ' ,]

        q_nc_bi_bo_ts_where = [ 'q_bool is not null and ' ,'q_binary is not null and ' ,'q_nchar is not null and ' ,'q_ts is not null and ' ,]
        
        q_where = random.sample(q_int_where,4) + random.sample(q_fl_do_where,2) + random.sample(q_nc_bi_bo_ts_where,2)

        q_in_where = ['q_bool in (0 , 1) ' ,  'q_bool in ( true , false) ' ,' (q_bool = true or  q_bool = false)' , '(q_bool = 0 or q_bool = 1)',]
        q_in = random.sample(q_in_where,1)
        
        return(q_where,q_in)

    def q_where_null(self):  

        q_int_where = ['q_bigint < -9223372036854775807 and ' , 'q_bigint > 9223372036854775807 and ','q_smallint < -32767 and ', 'q_smallint > 32767 and ',
        'q_tinyint < -127 and ' , 'q_tinyint > 127 and ' , 'q_int > 2147483647 and ' , 'q_int < -2147483647 and ',
        'q_bigint between  9223372036854775807 and -9223372036854775807 and ',' q_int between 2147483647 and -2147483647 and ',
        'q_smallint between 32767 and -32767 and ', 'q_tinyint between 127 and -127  and ',
        'q_bigint is null and ' , 'q_int is null and ' , 'q_smallint is null and ' , 'q_tinyint is null and ' ,]

        q_fl_do_where = ['q_float < -3.4E38 and ','q_float > 3.4E38 and ', 'q_double < -1.7E308 and ','q_double > 1.7E308 and ', 
        'q_float between 3.4E38 and -3.4E38 and ','q_double between 1.7E308 and -1.7E308 and ' ,
        'q_float is null and ' ,'q_double is null and ' ,]

        q_nc_bi_bo_ts_where = [ 'q_bool is null and ' ,'q_binary is null and ' ,'q_nchar is null and ' ,'q_ts is null and ' ,]
        
        q_where_null = random.sample(q_int_where,4) + random.sample(q_fl_do_where,2) + random.sample(q_nc_bi_bo_ts_where,2)

        q_in_where = ['q_bool in (0 , 1) ' ,  'q_bool in ( true , false) ' ,' (q_bool = true or  q_bool = false)' , '(q_bool = 0 or q_bool = 1)',]
        q_in_null = random.sample(q_in_where,1)

        return(q_where_null,q_in_null)

    def t_where(self):   
        t_int_where = ['t_bigint >= -9223372036854775807 and ' , 't_bigint <= 9223372036854775807 and ','t_smallint >= -32767 and ', 't_smallint <= 32767 and ',
        't_tinyint >= -127 and ' , 't_tinyint <= 127 and ' , 't_int <= 2147483647 and ' , 't_int >= -2147483647 and ',
        't_tinyint != 128 and ',
        't_bigint between  -9223372036854775807 and 9223372036854775807 and ',' t_int between -2147483647 and 2147483647 and ',
        't_smallint between -32767 and 32767 and ', 't_tinyint between -127 and 127  and ',
        't_bigint is not null and ' , 't_int is not null and ' , 't_smallint is not null and ' , 't_tinyint is not null and ' ,]

        t_fl_do_where = ['t_float >= -3.4E38 and ','t_float <= 3.4E38 and ', 't_double >= -1.7E308 and ','t_double <= 1.7E308 and ', 
        't_float between -3.4E38 and 3.4E38 and ','t_double between -1.7E308 and 1.7E308 and ' ,
        't_float is not null and ' ,'t_double is not null and ' ,]

        t_nc_bi_bo_ts_where = [ 't_bool is not null and ' ,'t_binary is not null and ' ,'t_nchar is not null and ' ,'t_ts is not null and ' ,]

        t_where = random.sample(t_int_where,4) + random.sample(t_fl_do_where,2) + random.sample(t_nc_bi_bo_ts_where,2)
        
        t_in = ['t_bool in (0 , 1) ' ,  't_bool in ( true , false) ' ,' (t_bool = true or  t_bool = false)' , '(t_bool = 0 or t_bool = 1)',]
        
        return(t_where,t_in)

    def t_where_null(self):   
        t_int_where = ['t_bigint < -9223372036854775807 and ' , 't_bigint > 9223372036854775807 and ','t_smallint < -32767 and ', 't_smallint > 32767 and ',
        't_tinyint < -127 and ' , 't_tinyint > 127 and ' , 't_int > 2147483647 and ' , 't_int < -2147483647 and ',
        't_bigint between  9223372036854775807 and -9223372036854775807 and ',' t_int between 2147483647 and -2147483647 and ',
        't_smallint between 32767 and -32767 and ', 't_tinyint between 127 and -127  and ',
        't_bigint is null and ' , 't_int is null and ' , 't_smallint is null and ' , 't_tinyint is null and ' ,]

        t_fl_do_where = ['t_float < -3.4E38 and ','t_float > 3.4E38 and ', 't_double < -1.7E308 and ','t_double > 1.7E308 and ', 
        't_float between 3.4E38 and -3.4E38 and ','t_double between 1.7E308 and -1.7E308 and ' ,
        't_float is null and ' ,'t_double is null and ' ,]

        t_nc_bi_bo_ts_where = [ 't_bool is null and ' ,'t_binary is null and ' ,'t_nchar is null and ' ,'t_ts is null and ' ,]

        t_where_null = random.sample(t_int_where,4) + random.sample(t_fl_do_where,2) + random.sample(t_nc_bi_bo_ts_where,2)
        
        t_in_where = ['t_bool in (0 , 1) ' ,  't_bool in ( true , false) ' ,' (t_bool = true or  t_bool = false)' , '(t_bool = 0 or t_bool = 1)',]
        t_in_null = random.sample(t_in_where,1)

        return(t_where_null,t_in_null)

    def hanshu_int(self):       
        hanshu = ['MIN','AVG','MAX','COUNT','SUM','STDDEV','FIRST','LAST','LAST_ROW','','SPREAD','CEIL','FLOOR','ROUND']
        column = ['(q_bigint)','(q_smallint)','(q_tinyint)','(q_int)','(q_float)','(q_double_null)','(q_bigint_null)','(q_smallint_null)','(q_tinyint_null)','(q_int_null)','(q_float_null)','(q_double_null)']        
        hanshu_column = random.sample(hanshu,1)+random.sample(column,1)
        hanshu_column = str(hanshu_column).replace("[","").replace("]","").replace("'","").replace(", ","")
        return hanshu_column

    def hanshu_all(self):       
        hanshu = ['COUNT','SUM','STDDEV','FIRST','LAST','LAST_ROW','','SPREAD','CEIL','FLOOR','ROUND']
        column = ['(*)','(q_bigint)','(q_smallint)','(q_tinyint)','(q_int)','(q_float)','(q_double_null)','(q_bigint_null)','(q_smallint_null)','(q_tinyint_null)','(q_int_null)','(q_float_null)','(q_double_null)']        
        hanshu_column_all = random.sample(hanshu,1)+random.sample(column,1)
        return hanshu_column_all

    # stable_group by.  table_ok
    def hanshu_stable(self):       
        hanshu = ['TWA','IRATE','STDDEV','INTERP','DIFF']
        column = ['(q_bigint)','(q_smallint)','(q_tinyint)','(q_int)','(q_float)','(q_double_null)','(q_bigint_null)','(q_smallint_null)','(q_tinyint_null)','(q_int_null)','(q_float_null)','(q_double_null)']        
        hanshu_column_stable = random.sample(hanshu,1)+random.sample(column,1)
        return hanshu_column_stable

    def column(self):
        int_column = ['(q_int)','(q_bigint)','(q_smallint)','(q_tinyint)','(q_float)','(q_double)','(q_int_null)','(q_bigint_null)','(q_smallint_null)','(q_tinyint_null)','(q_float_null)','(q_double_null)']
        bia_column = ['(*)','(_c0)','(_C0)','(q_bool)','(q_binary)','(q_nchar)','(q_ts)','(q_bool_null)','(q_binary_null)','(q_nchar_null)','(q_ts_null)']
        tag_column = ['(tbname)','(loc)','(t_int)','(t_bigint)','(t_smallint)','(t_tinyint)','(t_float)','(t_double)','(t_bool)','(t_binary)','(t_nchar)','(t_ts)']
        columns = int_column + bia_column + tag_column         

        if self.NUM == 1:
            columns = str(random.sample(int_column,1)+random.sample(bia_column,1)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 2:
            columns = str(random.sample(bia_column,2)+random.sample(tag_column,2)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 3:
            columns = str(random.sample(int_column,3)+random.sample(tag_column,3)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 4:
            columns = str(random.sample(columns,10)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        else:
            columns = str(random.sample(columns,5)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        
        return columns

    def column_hanshu(self):
        int_column = ['(q_int)','(q_bigint)','(q_smallint)','(q_tinyint)','(q_float)','(q_double)','(q_int_null)','(q_bigint_null)','(q_smallint_null)','(q_tinyint_null)','(q_float_null)','(q_double_null)']
        bia_column = ['(*)','(_c0)','(_C0)','(q_bool)','(q_binary)','(q_nchar)','(q_ts)','(q_bool_null)','(q_binary_null)','(q_nchar_null)','(q_ts_null)']
        tag_column = ['(tbname)','(loc)','(t_int)','(t_bigint)','(t_smallint)','(t_tinyint)','(t_float)','(t_double)','(t_bool)','(t_binary)','(t_nchar)','(t_ts)']
        columns = int_column + bia_column + tag_column

        hanshu_column = self.hanshu_int()

        hanshu_columns = ''
        for i in range(3):
            hanshu_column = self.hanshu_int()
            hanshu_columns += hanshu_column + ','            

        if self.NUM == 1:
            columns = str(random.sample(int_column,1)+random.sample(bia_column,1)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 2:
            columns = str(random.sample(bia_column,2)+random.sample(tag_column,2)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 3:
            columns = str(random.sample(int_column,3)+random.sample(tag_column,3)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 4:
            columns = str(random.sample(columns,10)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        elif self.NUM == 5 :
            columns = hanshu_column
        elif self.NUM == 6 :
            columns = hanshu_columns + hanshu_column
        else:
            columns = str(random.sample(columns,5)).replace("[","").replace("]","").replace("(","").replace(")","").replace("'","")
        
        return columns


    def time_window(self):       
        time = ['1','2','3','4','5','6','7','8','9','10']
        unit = ['a','s','m','h','d','w','n','y']
        td_base = str(random.sample(time,1)+random.sample(unit,1)).replace("[","").replace("]","").replace("'","").replace(", ","")
        
        td_interval = td_base
        td_interval = 'interval'+'(' +td_interval + ')'

        td_sliding = td_base
        td_sliding = 'sliding'+'(' +td_sliding + ')'

        fill = ['NONE','VALUE,100','PREV','NULL','LINEAR','NEXT']
        td_fill = str(random.sample(fill,1)).replace("[","").replace("]","").replace("'","").replace(", ","")
        td_fill = 'Fill' +'(' +td_fill + ')'

        td_session = td_base
        td_session = 'SESSION'+'(ts,'+td_session + ')'

        if self.NUM == 1:
            time_window = td_interval
        elif self.NUM == 2:
            time_window = td_interval + ' ' + td_sliding
        elif self.NUM == 3:
            time_window = td_fill 
        elif self.NUM == 4:
            time_window = td_interval + ' ' + td_fill 
        elif self.NUM == 5 :
            time_window = td_interval + ' ' + td_sliding + ' ' + td_fill 
        else:
            time_window = td_session
        
        return time_window

    def regular_where(self):       
        regular_q_where = self.q_where()
        
        q_where = random.sample(regular_q_where[0],5) 
        #q_where = str(q_where).replace("(","").replace(")","").replace("'","").replace("\"","").replace(",","").replace("[","").replace("]","")
        q_in_where = str(regular_q_where[1]).replace("[","").replace("]","").replace("'","")

        column = self.column()
        hanshu_column = self.hanshu_int()
        time_window = self.time_window()
        
        return(q_where,q_in_where,hanshu_column,time_window,column)

    def regular_where_null(self):       
        regular_q_where_null = self.q_where_null()
        
        q_where_null = random.sample(regular_q_where_null[0],5) 
        q_in_where_null = str(regular_q_where_null[1]).replace("[","").replace("]","").replace("'","")

        hanshu_column = self.hanshu_int()
        time_window = self.time_window()

        return(q_where_null,q_in_where_null,hanshu_column,time_window)

    def regular_where_all_and_null(self):   
        regular_q_where = self.q_where()
        
        q_where = random.sample(regular_q_where[0],5) 
        q_in_where = str(regular_q_where[1]).replace("[","").replace("]","").replace("'","")

        regular_q_where_null = self.q_where_null()
        
        q_where_null = random.sample(regular_q_where_null[0],5) 
        q_in_where_null = str(regular_q_where_null[1]).replace("[","").replace("]","").replace("'","")

        hanshu_column = self.hanshu_int()
        time_window = self.time_window()

        return(q_where,q_in_where,q_where_null,q_in_where_null,hanshu_column,time_window)

    def stable_where(self):       
        stable_q_where = self.q_where()
        stable_t_where = self.t_where()

        qt_where = random.sample(stable_q_where[0],3) + random.sample(stable_t_where[0],3)
        print(qt_where)
        qt_in_where = random.sample((stable_q_where[1] + stable_t_where[1]),1)
        print(qt_in_where)

        hanshu_column = self.hanshu_int()
        time_window = self.time_window()

        return(qt_where,qt_in_where,hanshu_column,time_window)

    # test >=0 <=0
    def regular_where_all_null(self):   
        q_where = self.q_where()
        
        q_where = random.sample(q_where[0],5) 
        q_in_where = random.sample(q_where[1],1)

        q_where_null = self.q_where()
        
        q_where = random.sample(q_where[0],5) 
        q_in_where = random.sample(q_where[1],1)

        q_int_where_add = ['q_bigint >= 0 and ' , 'q_smallint >= 0 and ', 'q_tinyint >= 0 and ' ,  'q_int >= 0 and ',
        'q_bigint between  0 and 9223372036854775807 and ',' q_int between 0 and 2147483647 and ',
        'q_smallint between 0 and 32767 and ', 'q_tinyint between 0 and 127  and ',
        'q_bigint is not null and ' , 'q_int is not null and ' ,]

        q_fl_do_where_add = ['q_float >= 0 and ', 'q_double >= 0 and ' , 'q_float between 0 and 3.4E38 and ','q_double between 0 and 1.7E308 and ' ,
        'q_float is not null and ' ,]

        q_nc_bi_bo_ts_where_add = ['q_nchar is not null and ' ,'q_ts is not null and ' ,]

        q_where_add = random.sample(q_int_where_add,2) + random.sample(q_fl_do_where_add,1) + random.sample(q_nc_bi_bo_ts_where_add,1)
        
        q_int_where_sub = ['q_bigint <= 0 and ' , 'q_smallint <= 0 and ', 'q_tinyint <= 0 and ' ,  'q_int <= 0 and ',
        'q_bigint between -9223372036854775807 and 0 and ',' q_int between -2147483647 and 0 and ',
        'q_smallint between -32767 and 0 and ', 'q_tinyint between -127 and 0 and ',
        'q_smallint is not null and ' , 'q_tinyint is not null and ' ,]

        q_fl_do_where_sub = ['q_float <= 0 and ', 'q_double <= 0 and ' , 'q_float between -3.4E38 and 0 and ','q_double between -1.7E308 and 0 and ' ,
        'q_double is not null and ' ,]

        q_nc_bi_bo_ts_where_sub = ['q_bool is not null and ' ,'q_binary is not null and ' ,]

        q_where_sub = random.sample(q_int_where_sub,2) + random.sample(q_fl_do_where_sub,1) + random.sample(q_nc_bi_bo_ts_where_sub,1)

        return(q_where_add,q_where_sub)

    def stable_where_all(self):  
        regular_where_all = self.regular_where_all()

        t_int_where_add = ['t_bigint >= 0 and ' , 't_smallint >= 0 and ', 't_tinyint >= 0 and ' ,  't_int >= 0 and ',
        't_bigint between  0 and 9223372036854775807 and ',' t_int between 0 and 2147483647 and ',
        't_smallint between 0 and 32767 and ', 't_tinyint between 0 and 127  and ',
        't_bigint is not null and ' , 't_int is not null and ' ,]

        t_fl_do_where_add = ['t_float >= 0 and ', 't_double >= 0 and ' , 't_float between 0 and 3.4E38 and ','t_double between 0 and 1.7E308 and ' ,
        't_float is not null and ' ,]

        t_nc_bi_bo_ts_where_add = ['t_nchar is not null and ' ,'t_ts is not null and ' ,]

        qt_where_add = random.sample(t_int_where_add,1) + random.sample(t_fl_do_where_add,1) + random.sample(t_nc_bi_bo_ts_where_add,1) + random.sample(regular_where_all[0],2)
        
        t_int_where_sub = ['t_bigint <= 0 and ' , 't_smallint <= 0 and ', 't_tinyint <= 0 and ' ,  't_int <= 0 and ',
        't_bigint between -9223372036854775807 and 0 and ',' t_int between -2147483647 and 0 and ',
        't_smallint between -32767 and 0 and ', 't_tinyint between -127 and 0 and ',
        't_smallint is not null and ' , 't_tinyint is not null and ' ,]

        t_fl_do_where_sub = ['t_float <= 0 and ', 't_double <= 0 and ' , 't_float between -3.4E38 and -1 and ','t_double between -1.7E308 and -1 and ' ,
        't_double is not null and ' ,]

        t_nc_bi_bo_ts_where_sub = ['t_bool is not null and ' ,'t_binary is not null and ' ,]

        qt_where_sub = random.sample(t_int_where_sub,1) + random.sample(t_fl_do_where_sub,1) + random.sample(t_nc_bi_bo_ts_where_sub,1) + random.sample(regular_where_all[1],2)

        qt_in = ['q_bool in (0 , 1) ' ,  'q_bool in ( true , false) ' ,' (q_bool = true or  q_bool = false)' , '(q_bool = 0 or q_bool = 1)', 't_bool in (0 , 1) ' ,  't_bool in ( true , false) ' ,' (t_bool = true or  t_bool = false)' , '(t_bool = 0 or t_bool = 1)',]

        hanshu_column = self.hanshu_int()

        return(qt_where_add,qt_where_sub,qt_in,hanshu_column)

    # test all and null
    def regular_where_all(self):     
        q_where = self.q_where()  

        q_where = random.sample(q_where[0],5) 
        q_in_where = random.sample(q_where[1],1)

        q_int_where_add = ['q_bigint >= 0 and ' , 'q_smallint >= 0 and ', 'q_tinyint >= 0 and ' ,  'q_int >= 0 and ',
        'q_bigint between  0 and 9223372036854775807 and ',' q_int between 0 and 2147483647 and ',
        'q_smallint between 0 and 32767 and ', 'q_tinyint between 0 and 127  and ',
        'q_bigint is not null and ' , 'q_int is not null and ' ,]

        q_fl_do_where_add = ['q_float >= 0 and ', 'q_double >= 0 and ' , 'q_float between 0 and 3.4E38 and ','q_double between 0 and 1.7E308 and ' ,
        'q_float is not null and ' ,]

        q_nc_bi_bo_ts_where_add = ['q_nchar is not null and ' ,'q_ts is not null and ' ,]

        q_where_add = random.sample(q_int_where_add,2) + random.sample(q_fl_do_where_add,1) + random.sample(q_nc_bi_bo_ts_where_add,1)
        
        q_int_where_sub = ['q_bigint <= 0 and ' , 'q_smallint <= 0 and ', 'q_tinyint <= 0 and ' ,  'q_int <= 0 and ',
        'q_bigint between -9223372036854775807 and 0 and ',' q_int between -2147483647 and 0 and ',
        'q_smallint between -32767 and 0 and ', 'q_tinyint between -127 and 0 and ',
        'q_smallint is not null and ' , 'q_tinyint is not null and ' ,]

        q_fl_do_where_sub = ['q_float <= 0 and ', 'q_double <= 0 and ' , 'q_float between -3.4E38 and 0 and ','q_double between -1.7E308 and 0 and ' ,
        'q_double is not null and ' ,]

        q_nc_bi_bo_ts_where_sub = ['q_bool is not null and ' ,'q_binary is not null and ' ,]

        q_where_sub = random.sample(q_int_where_sub,2) + random.sample(q_fl_do_where_sub,1) + random.sample(q_nc_bi_bo_ts_where_sub,1)

        return(q_where_add,q_where_sub)

    def stable_where_all(self):  
        regular_where_all = self.regular_where_all()

        t_int_where_add = ['t_bigint >= 0 and ' , 't_smallint >= 0 and ', 't_tinyint >= 0 and ' ,  't_int >= 0 and ',
        't_bigint between  0 and 9223372036854775807 and ',' t_int between 0 and 2147483647 and ',
        't_smallint between 0 and 32767 and ', 't_tinyint between 0 and 127  and ',
        't_bigint is not null and ' , 't_int is not null and ' ,]

        t_fl_do_where_add = ['t_float >= 0 and ', 't_double >= 0 and ' , 't_float between 0 and 3.4E38 and ','t_double between 0 and 1.7E308 and ' ,
        't_float is not null and ' ,]

        t_nc_bi_bo_ts_where_add = ['t_nchar is not null and ' ,'t_ts is not null and ' ,]

        qt_where_add = random.sample(t_int_where_add,1) + random.sample(t_fl_do_where_add,1) + random.sample(t_nc_bi_bo_ts_where_add,1) + random.sample(regular_where_all[0],2)
        
        t_int_where_sub = ['t_bigint <= 0 and ' , 't_smallint <= 0 and ', 't_tinyint <= 0 and ' ,  't_int <= 0 and ',
        't_bigint between -9223372036854775807 and 0 and ',' t_int between -2147483647 and 0 and ',
        't_smallint between -32767 and 0 and ', 't_tinyint between -127 and 0 and ',
        't_smallint is not null and ' , 't_tinyint is not null and ' ,]

        t_fl_do_where_sub = ['t_float <= 0 and ', 't_double <= 0 and ' , 't_float between -3.4E38 and -1 and ','t_double between -1.7E308 and -1 and ' ,
        't_double is not null and ' ,]

        t_nc_bi_bo_ts_where_sub = ['t_bool is not null and ' ,'t_binary is not null and ' ,]

        qt_where_sub = random.sample(t_int_where_sub,1) + random.sample(t_fl_do_where_sub,1) + random.sample(t_nc_bi_bo_ts_where_sub,1) + random.sample(regular_where_all[1],2)

        qt_in = ['q_bool in (0 , 1) ' ,  'q_bool in ( true , false) ' ,' (q_bool = true or  q_bool = false)' , '(q_bool = 0 or q_bool = 1)', 't_bool in (0 , 1) ' ,  't_bool in ( true , false) ' ,' (t_bool = true or  t_bool = false)' , '(t_bool = 0 or t_bool = 1)',]

        hanshu_column = self.hanshu_int()

        return(qt_where_add,qt_where_sub,qt_in,hanshu_column)        

    def stop(self):
        tdSql.close()
        tdLog.success("%s successfully executed" % __file__)


tdWhere_makesql = TDWhere_makesql()