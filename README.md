# Ahocorasick keyword match

```
    ____  __  ______  ___    __ __ __  ___
   / __ \/ / / / __ \/   |  / //_//  |/  /
  / /_/ / /_/ / /_/ / /| | / ,<  / /|_/ /
 / ____/ __  / ____/ ___ |/ /| |/ /  / /
/_/   /_/ /_/_/   /_/  |_/_/ |_/_/  /_/

```

**关键字快速查找匹配**

## 编译安装

```
$ git clone https://github.com/imaben/php-akm.git
$ cd php-akm
$ phpize
$ ./configure
$ make 
$ sudo make install
```

## php.ini配置
```
[akm]
extension=akm.so
akm.enable=On|Off
akm.dict_dir=/home/dict
```

说明：

- `akm.enable`表示扩展启用或关闭
- `akm.dict_dir`用来指定关键词词典所在的文件夹


## 函数说明

### akm_match

**关键词匹配**

```
array akm_match(string $dict_name, string $text)
```

**参数说明**

- `dict_name`：字典名称，即`akm.dict_dir`配置所在文件夹下的字典库名称（文件名）
- `text`：待匹配的文本

**返回值**

返回匹配含有`keyword`、`offset`、`extension`字段数组列表的二维数组，如：

```
[
	{
		"keyword" : "敏感词",
		"offset": 123,
		"extension": "扩展文本"
	},
	{
		"keyword" : "敏感词2",
		"offset": 1231,
		"extension": "扩展文本"
	}
]
```
说明：

- keyword:敏感词
- offset:敏感词所在文本中的位置
- extension:扩展文本

### akm_replace

**关键词替换**

```
int akm_replace(string $dict_name, string &$text, callable $callback)
```

**参数说明**

- `dict_name`: 字典名称，即`akm.dict_dir`配置所在文件夹下的字典库名称（文件名）
- `text`：待替换文本
- `callback`：处理匹配字符串的回调，接受三个参数
	- string `keyword`：匹配出的关键词
	- int `index`：关键词在文本中的位置
	- string `extension`：扩展文本
	
	如回调中返回一个字符串，则把匹配到的关键词替换成返回值。如无返回值，则不做任何处理

**返回值**

返回成功匹配的关键词个数

### akm_get_dict_list

**获取词典列表**

```
array akm_get_dict_list()
```

**返回值**

返回已索引的词典名称列表

## 字典数据结构

```
关键词|扩展文本
keyword1|extension_text1
keyword2|extension_text2
keyword3|extension_text3
```

说明：

- “|”为关键词和扩展文本之间的分割符
- “|”只对首行第一个有效，例“发票|政治|敏感”，则认定`发票`为关键词，`政治|敏感`为扩展文本
- 如无“|”符，则整行被认为一个关键词，返回时无扩展文本
- 每行定义一个关键词，空行自动跳过

## 性能测试

PC配置：
```
CPU:Intel(R) Core(TM) i5-4590 CPU @ 3.30GHz
内存：4GB*3 1600MHz
硬盘：东芝Q300
```

测试代码：

```
<?php
function getMillisecond() {
    list($t1, $t2) = explode(' ', microtime());
    return (float)sprintf('%.5f',(floatval($t1)+floatval($t2))*1000);
}

$dict_name = 'dict';
$text = file_get_contents("text.txt");

$start = getMillisecond();
$result = akm_match($dict_name, $text);
$end = getMillisecond();
echo '耗时' . ($end - $start) . "毫秒\n";
echo '内存占用:' . memory_get_usage() / 1024 / 1024 . "MB\n";
```

关键词数量：**45423**条

测试文本大小：**8KB**

测试结果：

第一次：
```
耗时0.488毫秒
内存占用:0.365MB
```

第二次：
```
耗时0.491毫秒
内存占用:0.365MB
```

第三次：
```
耗时0.462毫秒
内存占用:0.365MB
```
