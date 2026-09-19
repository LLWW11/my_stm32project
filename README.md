# weather-clock-FreeRTOS

B站梅花嵌入式的天气时钟项目代码，用的是正点原子stm32f407zgt6开发板，所以有点不一样，使用标准库➕FreeRTOS

使用的是weatherapi而非心知天气，因为可以免费通过IP定位查询当地天气但是返回的数据有点不一样，所以稍微修改了解析代码，保留了原始的心知天气parse代码，但是天气信息显示改了不少
这个是使用了lvgl库的版本，很多东西为了和非lvgl库兼容，大量使用了宏，还有一些没有改完，懒得改了，同时增加了二级页面，只不过显示的东西有限；

https://api.seniverse.com/v3/weather/now.json?key=[key]&location=fujianfuzhou&language=en&unit=c

http://api.weatherapi.com/v1/current.json?key=[key]&q=auto:ip&lang=zh_cn

官网：
https://www.seniverse.com/

https://www.weatherapi.com/

