定时调度管理模块(chron)
====================
chron程序是给联动管理提供定时功能.支持指定日期/星期周期/日期区间 功能.


 序号| 类型 |  时间  |      日   期           | 说明
-----|------|--------|------------------------|-----------------------------------------------
  1  |  0   | 12:34  | [2018-11-19]           | 指定日期: 代表2018年11月19日时间12:34时触发执行
  2  |  1   | 13:56  | [0,1,2,6]              | 星期周期: 代表每周日,周一,周二,周六时间为13:56时触发执行
  3  |  2   | 14:44  | [2018-8-28~2018-11-11] | 日期区间: 2018年8月28日到2018年11月11日之间每天14:44时触发执行


目录
--------------

* [开发准备](#开发准备)


* [接口功能](#接口功能)
    * [新建定时器任务](#新建定时器任务)
    * [删除定时器任务](#删除定时器任务)
    * [修改定时器任务](#修改定时器任务)
    * [查询定时器任务](#查询定时器任务)
    * [定时器触发通知](#定时器触发通知)
    * [错误信息](#错误信息)

* [其他](#其他)

## 开发准备
```
$ pwd
/home/nick/lede-17.01
$ mkdir -p package/leedarson
$ cd package/leedarson/
$ git clone https://github.com/nicklgw/openwrt-chron.git chron  #代码下载到本地
$ cd /home/nick/lede-17.01
$ make menuconfig
    选中 Leedarson --> <*> chron
$ make package/chron/{clean,compile,install} V=s    #单独编译chron模块
$ make V=s #编译整个系统固件

编译成功后将整个固件烧录到开发板中
编译后生成可执行文件 chron

```

## 接口功能
通过ubus查看chron对外提供的调用接口. 消息格式借鉴[JSON-RPC](http://www.jsonrpc.org/specification)
```
root@LEDE:~# ubus -v list chron
'chron' @1de922f6
	"create":{"type":"Integer","time":"String","date":"String","notice":"String"}
	"delete":{"id":"Integer"}
	"update":{"id":"Integer","attrs":"Table"}
	"query":{"id":"Integer"}
```
还有一个接口,当时间匹配时触发通知,  ubus命令测试
```
root@LEDE:~# ubus subscribe chron       # 订阅chron通知
{ "time_up": {"id":2,"notice":"I have a dream!"} }
```

### 新建定时器任务
- 新建指定日期定时器任务
```
root@LEDE:~# ubus call chron create '{"type":0, "time":"15:56","date": "[2018-2-3]", "notice":"I have a dream!"}'
{
	"result": {
		"id": 2
	}
}
```
- 新建星期周期定时器任务 
```
root@LEDE:~# ubus call chron create '{"type":1, "time":"16:00","date": "[0,1,6]", "notice":"I have a dream6!"}'
{
	"result": {
		"id": 3
	}
}
```
- 新建日期区间定时器任务 
```
root@LEDE:~# ubus call chron create '{"type":2, "time":"16:00","date": "[2018-2-1~2019-1-3]", "notice":"I have a dream6666!"}'
{
	"result": {
		"id": 4
	}
}
```

### 删除定时器任务
```
root@LEDE:~# ubus call chron delete '{"id":4}'
{
	"result": [
		"OK"
	]
}
```
### 修改定时器任务
可以修改定时任务的一个或多个属性, 可修改属性包括有: type, time, date, notice
```
root@LEDE:~# ubus call chron update '{"id":4, "attrs":{"type":0, "time":"16:11", "date":"[2018-2-3]", "notice":"dream"}}'
{
	"result": [
		"OK"
	]
}

root@LEDE:~# ubus call chron update '{"id":5, "attrs":{"time":"16:08"}}'
{
	"result": [
		"OK"
	]
}
```
### 查询定时器任务
可以查询单个或所有定时器任务
```
root@LEDE:~# ubus call chron query '{"id":5}'   #指定有效id,查询对应单个任务
{
	"result": {
		"id": 5,
		"type": 0,
		"time": "16:08",
		"date": "[2018-2-3]",
		"notice": "I have a dream6666!",
		"state": 0
	}
}
root@LEDE:~# ubus call chron query '{"id":-1}'        #当id=-1时,查询所有任务
{
	"result": [
		{
			"id": 1,
			"type": 0,
			"time": "15:02",
			"date": "[2018-2-2]",
			"notice": "dream666666666666",
			"state": 0
		},
		{
			"id": 2,
			"type": 0,
			"time": "15:56",
			"date": "[2018-2-3]",
			"notice": "I have a dream!",
			"state": 0
		},
		{
			"id": 3,
			"type": 1,
			"time": "16:00",
			"date": "[0,1,6]",
			"notice": "I have a dream6!",
			"state": 0
		},
		{
			"id": 5,
			"type": 0,
			"time": "16:08",
			"date": "[2018-2-3]",
			"notice": "I have a dream6666!",
			"state": 0
		}
	]
}
```
### 定时器触发通知
```
root@LEDE:~# ubus subscribe chron
{ "time_up": {"id":2,"notice":"I have a dream!"} }
{ "time_up": {"id":3,"notice":"I have a dream6!"} }
{ "time_up": {"id":5,"notice":"I have a dream6666!"} }
```
### 错误信息
当有错误信息时,返回格式: {"error":{"code":-10xx,"message":"error message"}}

|    error code      |       error message                    |         说明       |
--------------------|---------------|-------------------------------------------------
|CHRON_ERR_FAILED    |  Unknown error                      | 通用错误|
|CHRON_ERR_OK        |  OK                                 | 成功|
|CHRON_ERR_CREATE    |  Fail to create scheduled task      | 创建定时任务失败|
|CHRON_ERR_NOT_FOUND |  The specified ID not found         | 指定定时任务ID未找到|
|HRON_ERR_DELETE    |  Failed to delete scheduled task    | 删除定时任务失败|
|HRON_ERR_UPDATE    |  Failed to update scheduled task    | 更新定时任务失败|
|HRON_ERR_QUERY     |  Failed to query scheduled task     | 查询定时任务失败|

```
root@LEDE:~# ubus call chron query '{"id":4}'        #当id=4不存在时,查询出错
{
	"error": {
		"code": -10005,
		"message": "Failed to query scheduled task."
	}
}
```

## 其他

1. 从github上下载openwrt系统
```
$ git clone -b lede-17.01 https://github.com/openwrt/openwrt.git lede-17.01
```
2. 将编译生成的可执行程序chron拷贝到开发板
```
$ scp ./build_dir/target-arm_cortex-a53+neon-vfpv4_musl-1.1.16_eabi/chron/chron root@172.24.24.150:/root
```





