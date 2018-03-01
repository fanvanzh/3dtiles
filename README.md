# 3dtiles
a serials of tools for cesium 3dtiles, impl by rust and c++.

include these tools：

osgb => 3dtile , aim at having a high efficiency。

shapefile => 3dtile,  aim at large scale of buiding outline。

倾斜摄影 (osgb 格式) 转 3dtile 工具。

使用说明：

命令行运行： 
	3dtile.exe [FLAGS] [OPTIONS] --format <osgb,shape> --input <FILE> --output <FILE>

参数说明：
  -c, --tile config json <config>    Set the tile config:
	
                                     {
				     
                                      "x": x,
				      
                                      "y": y,
				      
                                      "offset": 0 (模型最低面地面距离),
				      
                                      "max_lvl" : 20 (处理切片模型到20级停止)
				      
                                     }
				     
  -f, --format <osgb,shape>          Set input format
  
  -i, --input <FILE>                 Set the input file
	
  -o, --output <FILE>                Set the out file


-c 在命令行传入 json 配置的字符串, json 内容为选配，可部分实现

-f 输入数据格式

-i 输入数据的目录，截止到 "\Data" 目录的上一级

-o 输出目录。最终结果位于输出目录的 "\Data" 目录


运行示例：

3dtile.exe -f osgb -i E:\Data\倾斜摄影\hgc -o E:\Data\倾斜摄影\hgc_test

3dtile.exe -f osgb -i E:\Data\倾斜摄影\dayanta -o E:\Data\倾斜摄影\dayanta_test -c "{\"offset\": 0}"
