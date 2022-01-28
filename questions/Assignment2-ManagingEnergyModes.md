Please include your answers to the questions below with your submission, entering into the space below each question
See [Mastering Markdown](https://guides.github.com/features/mastering-markdown/) for github markdown formatting if desired.

**Note: All average currents should be taken at a time scale of 200mS/div. See the pop-up menu in the lower-right corner of the Energy Profiler window**

**1. Fill in the below chart based on currents measured in each energy mode, replacing each TBD with measured values.  Use the [Selecting Ranges](https://www.silabs.com/documents/public/user-guides/ug343-multinode-energy-profiler.pdf) feature of the profiler to select the appropriate measurement range.  Your measurements should be accurate to 10%**

Energy Mode | Period (ms) | LED On Time (ms) |Period average current (uA) | Average Current with LED off (uA) | Average Current with LED On (uA)
------------| ------------|------------------|----------------------------|-----------------------------------|---------------------------------
EM0         |    2240     |       170        |          5050              |           5020                    |         5500
EM1         |    2250     |       172        |          3590              |           3550                    |         4020
EM2         |    2240     |       176        |          42.62             |           1.99                    |         492.10
EM3         |    2210     |       176        |          43.69             |           1.91                    |         495.46



**2. ScreenShots**  

***EM0***  
Period average current    
![em0_avg_current_period][em0_avg_current_period]  
Average Current with LED ***off***  
![em0_avg_current_ledoff][em0_avg_current_ledoff]  
Average Current with LED ***on***  
![em0_avg_current_ledon][em0_avg_current_ledon]  

***EM1***  
Period average current    
![em1_avg_current_period][em1_avg_current_period]  
Average Current with LED ***off***  
![em1_avg_current_ledoff][em1_avg_current_ledoff]  
Average Current with LED ***on***  
![em1_avg_current_ledon][em1_avg_current_ledon]  

***EM2***  
Period average current  
![em2_avg_current_period][em2_avg_current_period]  
Average Current with LED ***off***  
![em2_avg_current_ledoff][em2_avg_current_ledoff]  
Average Current with LED ***on***  
![em2_avg_current_ledon][em2_avg_current_ledon]   
LED measurement - Period   
![em2_led_period][em2_led_period]  
LED measurement - LED on time   
![em2_led_ledOnTime][em2_led_ledOnTime]  

***EM3***  
Period average current    
![em3_avg_current_period][em3_avg_current_period]  
Average Current with LED ***off***  
![em3_avg_current_period][em3_avg_current_ledoff]   
Average Current with LED ***on***  
![em3_avg_current_period][em3_avg_current_ledon]   
LED measurement - Period   
![em3_led_period][em3_led_period]  
LED measurement - LED on time   
![em3_led_ledOnTime][em3_led_ledOnTime]  

[em0_avg_current_period]: screenshots/a2/em0_avg_current_period.png "em0_avg_current_period"
[em0_avg_current_ledoff]: screenshots/a2/em0_avg_current_ledoff.png "em0_avg_current_ledoff"
[em0_avg_current_ledon]: screenshots/a2/em0_avg_current_ledon.png "em0_avg_current_ledon"

[em1_avg_current_period]: screenshots/a2/em1_avg_current_period.png "em1_avg_current_period"
[em1_avg_current_ledoff]: screenshots/a2/em1_avg_current_ledoff.png "em1_avg_current_ledoff"
[em1_avg_current_ledon]: screenshots/a2/em1_avg_current_ledon.png "em1_avg_current_ledon"

[em2_avg_current_period]: screenshots/a2/em2_avg_current_period.png "em2_avg_current_period"
[em2_avg_current_ledoff]: screenshots/a2/em2_avg_current_ledoff.png "em2_avg_current_ledoff"
[em2_avg_current_ledon]: screenshots/a2/em2_avg_current_ledon.png "em2_avg_current_ledon"
[em2_led_period]: screenshots/a2/em2_avg_current_period.png "em2_led_period"
[em2_led_ledOnTime]: screenshots/a2/em2_avg_current_ledon.png "em2_led_ledOnTime"

[em3_avg_current_period]: screenshots/a2/em3_avg_current_period.png "em3_avg_current_period"
[em3_avg_current_ledoff]: screenshots/a2/em3_avg_current_ledoff.png "em3_avg_current_ledoff"
[em3_avg_current_ledon]: screenshots/a2/em3_avg_current_ledon.png "em3_avg_current_ledon"
[em3_led_period]: screenshots/a2/em3_avg_current_period.png "em3_led_period"
[em3_led_ledOnTime]: screenshots/a2/em3_avg_current_ledon.png "em3_led_ledOnTime"
