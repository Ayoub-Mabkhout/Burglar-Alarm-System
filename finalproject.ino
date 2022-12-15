/*
CSC 4328 Embedded Systems Final Project: Burglar Alarm System
Made by Ayoub Mabkhout
Supervised by Dr. James C. Peyton-Jones

Implemented Core Project + Additional Features i, ii, iii, iv, vi, viii, and ix
Also implemented a custom additional feature (x): The LED flashes at 
the same rate as the frequency of the buzzer changes in alarm mode
*/

#define BTN1 A1                                                 // BTN1 input in PIN A1
#define RLS A4                                                  // RLS input in PIN A4
#define BUZZER D1                                               // BUZZER output in PIN D1
#define LED0 D0                                                 // LED output in pin D0

SYSTEM_THREAD(ENABLED);                                         // Allows for the OS to manage system background processing 
                                                                // (such as managing concurrent threads) without blocking main loop

enum states {OFF, ARMED, ALARM};                                // ENUM type for the alarm states (FSM states)
enum inputs {BUTTON, CLOUD, INTRUDER_IN, INTRUDER_OUT};         // ENUM type for the types of input the alarm FSM can take

states state;
inputs input;
bool stateFSMFlag = false;                                      // flag set by the interrupts to enable the FSM to be executed in the loop
bool alarmEnabled = false;                                      // flag set by the FSM to indicate alarm mode for our alarm timer function
long lastPressed = millis();                                    // contains the value of the last time the button was pressed, used to debounce the button

String cloudKey = "m71xNJ";                                     // cloud key to arm/disarm hardcoded into the device

Timer alarmTimer(450, alarmFunction, false);                    // alarm timer that flashes the LED and sounds 
                                                                // the buzzer with a HIGH or LOW pitch tone
Timer presenceTimer(3000,setIntruderIn,true);                   // timeout function to set the alarm after the intruder has been present for 3s
// Timer absenceTimer(1050,setIntruderOut,true);                    // timeout function to stop and re-arm the alarm after the intruder has left for 20s
Timer absTimer(10000,absTimerISR,true);



void stateFSM(){                                                // State FSM as described in the FSM Drawing
                                                                // State FSM is mostly a Moore FSM with one exception
    bool alarm_on_to_off = false;                               // boolean variable set during transition (akin to Mealey FSM)
                                                                // to indicate that the FSM transitioned FROM the ALARM state to anotgehr state
    
    // FSM Transition switch
    switch(state){
        case OFF:
            if(input == BUTTON || input == CLOUD)
                state = ARMED;
            break;
            
        case ARMED:
            if(input == INTRUDER_IN)
                state = ALARM;
            else if(input == BUTTON || input == CLOUD)
                state = OFF;
            break;
        
        case ALARM:
            if(input == BUTTON || input == CLOUD)
                state = OFF;
            else if(input == INTRUDER_OUT)
                state = ARMED;
            alarm_on_to_off = true;                             // ALARM => ARMED || ALARM => OFF
            break;
            
        default:
            state = OFF;
        
    }
    
        // FSM execution switch
    switch(state){                                              // the main variable being set (output of the FSM) is alarmEnabled
        case OFF:
            if(alarm_on_to_off)                                 // disable alarm but not btn confirmation tone
                noTone(BUZZER);                                 
            alarmEnabled = false;
            Particle.publish("BAS Disarmed.",Time.timeStr());   // also publishing an event to the cloud for each state
            break;
        
        case ARMED:
            if(alarm_on_to_off)                                 // disable alarm but not btn confirmation tone
                noTone(BUZZER);
            alarmEnabled = false;
            Particle.publish("BAS ARMED.",Time.timeStr());
            break;
        
        case ALARM:
            alarmEnabled = true;
            Particle.publish("BAS: Intruder Detected!",Time.timeStr());
    }
}


void alarmFunction(void){                                   // a function that toggles LED0 on and off and BUZZER HIGH/LOW pitch tone
                                                            // when alarm Enabled is low
        static bool toneHigh = true;                        // static variable to keep track of the pitch value of the tone between iterations
        if(!alarmEnabled){
           digitalWrite(LED0, LOW);                         // I avoid to put noTone() here since it would disable the btn
                                                            // confirmation tones
           return; 
        } 
        
        int LED0Val = digitalRead(LED0);                    // variable to keep track of the digital value written to the LED
                                                            // in order to write the opposite value
        if(toneHigh)
            tone(BUZZER,12000,0);                           // set tone to HIGH pitch at 12KHz
        else
            tone(BUZZER,100,0);                             // set tone to LOW pitch at 100Hz
        
        toneHigh != toneHigh;                               // negate the tone pitch variable 
        
        digitalWrite(LED0,!LED0Val);                        // turn LED0 in pin D7 ON/OFF             
}


void btnISR(){                                              // input work with interrupts instead of polling, ISR's have to be carefully designed
    if(millis()-lastPressed < 350)                          // in order to debounce the button, we record the time of the last valid press
        return;                                             // and check if more than 350ms has passed, if not then this current button press
                                                            // wil just get ignored
    lastPressed = millis();                                 // If btn press is valid, it becomes the new "last" instance of the btn being pressed
                                                            // => update the value of lastPressed to current time
    
    input = BUTTON;                                         // input to the FSM of type BUTTON
    tone(BUZZER,400,100);                                   // confirmation tone: 400Hz for 100ms
    stateFSMFlag = true;                                    // enable flag to execute FSM in main loop
}

void rlsISR(){                                              // RLS ISR launches or stops the absence and presence timers, 
                                                            // which will eventually update the input for the FSM
    if(!digitalRead(RLS)){                                  // if no one is detected, disable the absence timer and state the presence timer
                                                            // to start counting 3 seconds of the intruder being present
        if(absTimer.isActive())
            absTimer.stopFromISR();
        presenceTimer.resetFromISR();
    }
    else{                                                   // no intruder detected => stop the presence timer and start the absence timer
                                                            // to start counting for 20s of the intruder being absent
        if(presenceTimer.isActive())
            presenceTimer.stopFromISR();
        absTimer.resetFromISR();                            // ...FromISR() suffix to timer functions following particle io's best practices
    }
}

void absTimerISR(){                                         // ISR for the absence timer, called when intruder absent for 20s
    input = INTRUDER_OUT;                                   //  => input becomes INTRUDER_OUT + execute FSM
    stateFSMFlag = true;
}

void setIntruderIn(){                                       // ISR for the presence timer, called when intruder present for 3s
    input = INTRUDER_IN;                                    // => input becomes INTRUDER_IN + execute FSM
    stateFSMFlag = true;
}

void setIntruderOut(){                                      // ISR for the absence timer, called when intruder absent for 20s
    input = INTRUDER_OUT;                                   //  => input becomes INTRUDER_OUT + execute FSM
    stateFSMFlag = true;
}

int cloudArmDisarm(String code){                            // ISR for cloud function to ARM/DISARM the BAS
    if(code.equals(cloudKey)){                              // check the cloud key given as argument
        input = CLOUD;                                      // if true then the result is the same as if the button was pressed
        stateFSMFlag = true;                                // execute FSM with input = BUTTON
        return 0;
    }
    else{
        Particle.publish("Wrong key");                      // otherwise notify the cloud user that the given key was wrong
        return 1;
    }
}

// DEBUG SECTION
bool absenceTimerActive(){
    return absTimer.isActive();
}

bool presenceTimerActive(){
    return presenceTimer.isActive();
}
// DEBUG SECTION

void setup() {
    pinMode(BUZZER,OUTPUT);                                 // Buzzer in output mode so that we can write to it using tone()
    pinMode(LED0,OUTPUT);                                   // LED in output mode so that we can write to it using digitalWrite()
    pinMode(BTN1, INPUT_PULLDOWN);                          // BTN in input pulldown mode since it is active high
    pinMode(RLS, INPUT_PULLUP);                             // RLS in input pullup mode since it active low
                                                            // RLS low => intruder detected
    state = OFF;                                            // FSM state initialized at 'OFF'
    attachInterrupt(BTN1, btnISR, RISING);                  // attaching BTN ISR to the Button pin on a Rising Edge Trigger
    attachInterrupt(RLS, rlsISR, CHANGE);                   // attaching the RLS ISR to the RLS pin with the CHANGE option
                                                            // in order to detect both H-to-L (intruder detected) and 
                                                            // L-to-H (intruder gone)
    alarmTimer.start();                                     // alarm timer function, executes the actual alarm when alarmEnabled
    Particle.function("Arm-Disarm BAS",cloudArmDisarm);     // cloud function to ARM/DISARM the BAS, accessible through particle console
    
    // DEBUG SECTION
    Particle.variable("alarmEnabled",alarmEnabled);
    Particle.variable("Absence Timer ON",absenceTimerActive);
    Particle.variable("Presence Timer ON",presenceTimerActive);
    // DEBUG SECTION
    

}

void loop() {                                               // nearly all the different sections of the code are asynchronous
    if(stateFSMFlag){                                       // not much going on in the main loop except for executing the FSM when prompted
        stateFSM();                                         // Execute FSM if flag is true, flag is set by the ISR's of relevant inputs
        stateFSMFlag = false;                               // reset flag after FSM is exected to not execute it more than once for the same input
    }
}
