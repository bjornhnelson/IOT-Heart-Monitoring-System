Please include your answers to the questions below with your submission, entering into the space below each question
See [Mastering Markdown](https://guides.github.com/features/mastering-markdown/) for github markdown formatting if desired.

**1. How much current does the system draw (instantaneous measurement) when a single LED is on with the GPIO pin set to StrongAlternateStrong?**
   Answer: 5.57 mA


**2. How much current does the system draw (instantaneous measurement) when a single LED is on with the GPIO pin set to WeakAlternateWeak?**
   Answer: 5.54 mA


**3. Is there a meaningful difference in current between the answers for question 1 and 2? Please explain your answer, 
referencing the [Mainboard Schematic](https://www.silabs.com/documents/public/schematic-files/WSTK-Main-BRD4001A-A01-schematic.pdf) and [AEM Accuracy](https://www.silabs.com/documents/login/user-guides/ug279-brd4104a-user-guide.pdf) section of the user's guide where appropriate. Extra credit is avilable for this question and depends on your answer.**
   Answer: No, there is not a meaningful difference in current between the StrongAlternateStrong and WeakAlternateWeak configurations.  There are several potential explanations for this.
   
   The energy profiler shows the current from the entire MCU. In the StrongAlternateStrong configuration, the current varies from 5.05 mA to 5.57 mA when the LED is off or on.
   In the WeakAlternateWeak configuration, the current varies from 5.04 mA to 5.54 mA when the LED is off or on. This shows that powering one LED only requires around 0.5 mA of current.
   
   Looking at the datasheet, each LED is connected in series to a 3K resistor. Assuming a voltage drop of 3V on an LED, by applying Ohm's law the current through an LED would be 1 mA.
   According to the em_gpio.h file, the maximum GPIO drive strength is 1 mA in WeakAlternateWeak and 10 mA in StrongAlternateStrong.
   The 0.5 mA measured current is below either of these maximum values, which explains why there is not a significant difference between configurations.
   
   One other potential factor is the energy profiler's margin of error. The user guide states that the AEM is accurate within 0.1 mA, which is a relatively large portion (20% of 0.5 mA) of the current driving an LED.
   Therefore, the accuracy of the AEM could also play a role in the similar current values that were measured.


**4. With the WeakAlternateWeak drive strength setting, what is the average current for 1 complete on-off cycle for 1 LED with an on-off duty cycle of 50% (approximately 1 sec on, 1 sec off)?**
   Answer: 5.30 mA
   (min -> max = 5.04 mA -> 5.54 mA)


**5. With the WeakAlternateWeak drive strength setting, what is the average current for 1 complete on-off cycle for 2 LEDs (both on at the time same and both off at the same time) with an on-off duty cycle of 50% (approximately 1 sec on, 1 sec off)?**
   Answer: 5.25 mA
   (min -> max = 4.76 mA -> 5.73 mA)

