
关闭回显
ATE0
41 54 45 30 0D 0A 

配置固定波特率
AT+IPR=115200&W
41 54 2B 49 50 52 3D 31 31 35 32 30 30 26 57 0D 0A 

首先要确保 SIM 卡的 PIN 码已解
AT+CPIN?
41 54 2B 43 50 49 4E 3F 0D 0A 

确认找网成功
AT+CREG?
41 54 2B 43 52 45 47 3F 0D 0A 

查询 GPRS 附着是否成功
AT+CGATT?
41 54 2B 43 47 41 54 54 3F 0D 0A 

将 Context 0 设为前台 Context。
AT+QIFGCNT=0
41 54 2B 51 49 46 47 43 4E 54 3D 30 0D 0A 

设置 GPRS 的 APN。
AT+QICSGP=1,"CMNET"
41 54 2B 51 49 43 53 47 50 3D 31 2C 22 43 4D 4E 45 54 22 0D 0A 

接收到数据后，输出提示:" +QIRDI: <id>,<sc>,<sid>"。
AT+QINDI=1
41 54 2B 51 49 4E 44 49 3D 31 0D 0A 

在接收到的数据之前增加头信息"IPD<len>:"
AT+QIHEAD=1
41 54 2B 51 49 48 45 41 44 3D 31 0D 0A 

在接收到的数据头位置增加数据来源的地址和端口号
AT+QISHOWRA=1
41 54 2B 51 49 53 48 4F 57 52 41 3D 31 0D 0A 

在接收到的数据之前增加传输层的协议类型
AT+QISHOWPT=1
41 54 2B 51 49 53 48 4F 57 50 54 3D 31 0D 0A

非透传模式
AT+QIMODE=0
41 54 2B 51 49 4D 4F 44 45 3D 30 0D 0A 

建立 TCP 连接
AT+QIOPEN="TCP","42.121.115.112","56673"
41 54 2B 51 49 4F 50 45 4E 3D 22 54 43 50 22 2C 22 34 32 2E 31 32 31 2E 31 31 35 2E 31 31 32 22 2C 22 35 36 36 37 33 22 0D 0A 

发送数据
AT+QISEND=3
41 54 2B 51 49 53 45 4E 44 3D 33 0D 0A 

输入数据
123
31 32 33

查询数据发送情况
AT+QISACK
41 54 2B 51 49 53 41 43 4B 0D 0A 

读取数据
AT+QIRD=0,1,0,1024
41 54 2B 51 49 52 44 3D 30 2C 31 2C 30 2C 31 30 32 34 0D 0A 

关闭TCP连接
AT+QICLOSE
41 54 2B 51 49 43 4C 4F 53 45 0D 0A 


/////////////////////////////////////////////////////
数据抓取：
+QIRD: 42.121.115.112,56673,TCP,29
79879879879879879879878979887
OK

