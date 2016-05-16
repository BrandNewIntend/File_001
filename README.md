#   WIFI_Firmware-->APP 通讯数据  #
 
 交互途径: 通过云端桥接,需要遵守云端数据格式.  
 产品原型: 电热
  
#### 实际状态描述:
  ```
  字段:        数值 
  Code:      0- NoErro 故障代码:1-干烧  2-传感器故障 3-超温
  OnOff      0-关机, 1-开机 
  State:     0-加热中, 1-待机 
  TempCur:   0--100 当前实际温度 
  Appoint:   0-关闭预约 1--开启预约 
  ```


#### 设置状态描述:
 ```
  字段:  数值 
  TempSet:  30--75   设置温度 (Step=1)
  Power:    1,2,3      设置功率 KW 
  Timer:    00:00---24:00 预约时间 (Step=10min) 
  Mode:     1--普通模式,2--无, 3--夜电模式 
  ```

## WIFI_Firmware-->APP 例子:
```
{
		"Status":{
		  "Code":0,
      "OnOff":1,
		  "State":1,
		  "TempCur":28,
		  "Appoint":1,
		  "TempSet":41,
		  "Power":2,
		  "Mode":1,
		  "Timer":"19:30"
		}
}

```

#  APP -->WIFI_Firmware 通讯数据
#### 设置状态描述:  
```
  字段:       数值 
  OnOff  :  0-关机, 1-开机
  Appoint:  0-关闭预约 1--开启预约   
  TempSet:  30--75 (Step=1)   设置温度 
  Power:    1,2,3      设置功率 KW 
  Timer:    00:00---24:00  字符串, (Step=10min) 
  Mode:     1--普通模式,2--无,  3--夜电模式 
```

APP-->WIFI_Firmware 例子:
```
{
	"Status":{
    "OnOff":1,
	  "Appoint":1,
	  "TempSet":41,
	  "Power":2,
	  "Mode":1,
	  "Timer":"19:30"
	}
}
```