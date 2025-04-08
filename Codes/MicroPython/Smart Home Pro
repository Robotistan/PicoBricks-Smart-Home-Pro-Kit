# Libraries
from picobricks import SSD1306_I2C, WS2812, SHTC3, NEC_16, IR_RX, MotorDriver
from machine import Pin, I2C, PWM, ADC, UART, Timer
import utime
import time

# OLED display dimensions
WIDTH  = 128 
HEIGHT = 64

# IR remote control variables
ir_data = 0
data_rcvd = False

# Password settings for the lock
correct_password = [1, 2, 3, 4] 
password = [0,0,0,0]

# Various control variables
lock_state = 0   
buttonReleased = 1
passIndex = 0
digitCounter = 1
user_soil = 0
user_fan = 0
oldDigit = 0
control = 0
button_press_time = None 

# Melody settings
melody = [262, 330, 392, 392, 330, 262]
duration = 0.3

doorClosed = 170
doorOpen = 90
windowClosed = 0
windowOpen = 90

tempThreshold = 26
ldrThreshold = 8000
soilThreshold = 50

# Function to lock the safe
def lockTheSafe():
    oled.fill(0)
    oled.text("Locking...", 30, 20)
    oled.show()
    utime.sleep(0.3)
    motor.servo(1, doorClosed) # Move servo to locked position
    motor.servo(2,windowClosed)
    led.low()

# Function to unlock the safe
def unlockTheSafe():
    led.high()
    oled.fill(0)
    oled.text("Unlocked!", 30, 20)
    oled.show()
    utime.sleep(0.3)
    motor.servo(1, doorOpen) # Move servo to unlocked position
    motor.servo(2,windowOpen)
    utime.sleep(5)
    lockTheSafe()  # Relock after 5 seconds

# Function to check if entered password is correct
def passwordCheck(definedPassword, enteredPassword):
    for i in range(len(definedPassword)):
        if definedPassword[i] != enteredPassword[i]:
            return False
    return True

# Callback function for IR remote signals
def ir_callback(data, addr, ctrl):
    global ir_data
    global ir_addr, data_rcvd
    if data > 0:
        ir_data = data
        ir_addr = addr
        print('Data {:02x} Addr {:04x}'.format(data, addr))
        data_rcvd = True

# Function to play a melody
def play_melody():
    for note in melody:
        buzzer.duty_u16(32767) 
        buzzer.freq(note)              
        time.sleep(duration)       
        buzzer.duty_u16(0)        
        time.sleep(0.05)

# Initialize hardware components
i2c = I2C(0, scl=Pin(5), sda=Pin(4))
ws2812 = WS2812(6,brightness=1)
ir = NEC_16(Pin(0, Pin.IN), ir_callback)
motor = MotorDriver(i2c)
shtc = SHTC3(i2c)
oled = SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=0x3c)
button = Pin(10, Pin.IN, Pin.PULL_UP)                                                    
buzzer = PWM(Pin(20))
soil_sensor = ADC(Pin(28))
ldr = ADC(Pin(27))
pot = ADC(Pin(26))
led = Pin(7, Pin.OUT)

#Close window and door
motor.servo(4, doorClosed)
motor.servo(2, windowClosed)

while True:
    # Read temperature and humidity from SHTC3 sensor
    temp = shtc.temperature()
    hum = shtc.humidity()
    
    # Display temperature and humidity on OLED
    if control == 0:
        oled.fill(0)
        oled.show()
        oled.text("Temp: {:.1f}C".format(temp), 10, 10)
        oled.text("Humidity: {:.1f}%".format(hum), 10, 30)
        oled.show()

    # Handle button press logic for melody activation
    if button.value() == 1 and control==0: 
        if button_press_time is None:  
            button_press_time = utime.ticks_ms()  
    elif button.value() == 0:  
        if button_press_time is not None:
            press_duration = utime.ticks_diff(utime.ticks_ms(), button_press_time)  
            if press_duration >= 1000:
                play_melody()
                control = 1
                
            button_press_time = None  

    # Read light sensor value and adjust LED brightness accordingly
    ldr_value = ldr.read_u16()
    if ldr_value < ldrThreshold:
        ws2812.pixels_fill((255, 255, 255))
        ws2812.pixels_show()
    else:
        ws2812.pixels_fill((0, 0, 0))
        ws2812.pixels_show()
        
    # Process IR remote commands
    if data_rcvd == True:
        data_rcvd = False
    
        if ir_data == IR_RX.number_1:
            motor.servo(4,doorOpen)  # Open door
        elif ir_data == IR_RX.number_2:
            motor.servo(4,doorClosed)  # Close door
        elif ir_data == IR_RX.number_3:
            motor.dc(1,50,1)  # Activate soil moisture motor
            user_soil = 1
        elif ir_data == IR_RX.number_4:
            motor.dc(1,0,0)  # Deactivate soil moisture motor
            user_soil = 0
        elif ir_data == IR_RX.number_5:
           motor.dc(2,80,1)  # Activate fan
           user_fan = 1
        elif ir_data == IR_RX.number_6:
            motor.dc(2,0,0)  # Deactivate fan
            user_fan = 0
        elif ir_data == IR_RX.number_7:
            motor.servo(2,windowOpen)  # Open window
        elif ir_data == IR_RX.number_8:
            motor.servo(2,windowClosed)  # Close window
        elif ir_data == IR_RX.number_9:
            play_melody()
    
    # Automatically activate fan based on temperature
    if  temp >= tempThreshold and user_fan == 0:
        motor.dc(2,80,1)  # Turn on fan
      
    elif temp < tempThreshold and user_fan ==0:
        motor.dc(2,0,0)  # Turn off fan
    
    # Read soil moisture level 
    soil_value = soil_sensor.read_u16() 
    soil_moisture = (soil_value / 65535) * 100
    
    if soil_moisture <= soilThreshold and user_soil == 0:
        motor.dc(1,50,1)
    elif soil_moisture > soilThreshold and user_soil == 0:
        motor.dc(1,0,0)
    
    # Control door
    if control == 1:
        if lock_state:
            lockTheSafe()
            lock_state = 0
        else:
            digit = int((pot.read_u16()*10)/65536)
        
            if digit != oldDigit:
                oldDigit = digit
                oled.fill(0)
                oled.text("Password",30,10)
                oled.hline(25, 40, 9, 0xffff)
                oled.hline(50, 40, 9, 0xffff)
                oled.hline(75, 40, 9, 0xffff)
                oled.hline(100, 40, 9, 0xffff)
                
                for c in range (digitCounter-1):
                    oled.text(str(password[c]),25*(c+1),30)
                oled.text(str(digit),25*digitCounter,30)
                oled.show()
            
            if button.value() == 0 and buttonReleased == 0:     # button released  (for latch detection)
                buttonReleased = 1
            
            if button.value() == 1 and buttonReleased == 1:     # button pressed (for latch detection)
                buttonReleased = 0
                password[passIndex] = digit
                digitCounter += 1
                oldDigit = 0

                if passIndex >= 3:
                    passIndex = 0
                    digitCounter = 1
                    if (passwordCheck(correct_password, password)):
                        unlockTheSafe()
                        lock_state = 1
                        control=0
                    else:
                        oled.fill(0)
                        oled.text("Try Again",30,20)
                        oled.show()
                        control=0
                else:
                    passIndex += 1
