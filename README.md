# Mechatronics_with_MSP430G2553
This repository contains code from the take-home portions of Mechatronics with Prof. Dan Block at UIUC. The take-home projects worked with the MSP430G2553 microcontroller which students soldered contacts for on a PCB to connect it to various sensors and outputs. Course content available at http://coecsl.ece.illinois.edu/ge423/ although this link gets updated each semester.  
  
Please do not steal the code if you are currently in the course, there is no guarentee that any of it is correct or up to date.  

## Content
Each project is fairly small and contained in its own folder. The folder will contain a video of the project working if one is available from when I was taking the course. Unless otherwise specified, the only file I worked in will be named <code>user_<em>folderName</em>.c</code> where <em>folderName</em> is the name of the folder it is found under. A skeleton of that file and all the other files are initialized by running the executable <code>G2553ProjCreator.exe</code> under the folder <code>G2553ProjCreator</code>, which was provided by the course.  
  
chasingLEDs: Introduces bit manipulation, the timer function, and UART in C to make 4 LEDs chase each other back and forth  
getSwitchState: Focuses on inputs and space conservation by using char's (as opposed to ints) to label the current state of 2 buttons and use LEDs to indicate the state  
readPotWithADC: Starts working with setting up the ADC to read data, in this case the voltage going through a potentiometer  
readPhotoresistorWithADC: Establishes the importance of tuning sensors to the environment they will be functioning in by controlling LEDs based on if the lights in the room are on or off (as indicated by a photoresistor)  

## Setup
All the code done in this class was edited and debugged using Code Composer Studio and a terminal emulator such as Tera Term or PuTTY. The terminal emulator of choice needs to be set to communicate with the COM port that the MSP430 is plugged into via USB cable and to be communicating at 115200 bits/second. The COM port can by determined by searching through your computer's Device Manager. Once this is set up and the code is open in Code Composer, build the project by clicking the hammer icon in the toolbar or clicking <code>ctrl+b</code>. The program will flash the microcontroller and once it does, the green play button at the top can be pressed to start running the code.
