// ***************************************************************************************
// Sample RFM69 sketch for Moteino to illustrate sending and receiving, button interrupts
// ***************************************************************************************
// When you press the button on the SENDER Moteino, it will send a short message to the
// RECEIVER Moteino and wait for an ACK (acknowledgement that message was received) from 
// the RECEIVER Moteino. If the ACK was received, the SENDER will blink the onboard LED
// a few times. The RECEIVER listens to a specific token, and it alternates the onboard LED
// state from HIGH to LOW or vice versa whenever this token is received.
// ***************************************************************************************
// Hardware setup:
// ***************************************************************************************
// On the sender, hook up a momentary tactile button to D3 like this:
//          __-__
//        __|   |___
// GND ----> BTN ----> D3
// Load this sketch on the RECEIVER with NODEID=RECEIVER (adjust in config section below)
// Load this sketch on the SENDER with NODEID=SENDER (adjust in config section below)
// RFM69 library and code by Felix Rusu - felix@lowpowerlab.com
// Get libraries at: https://github.com/LowPowerLab/
// Make sure you adjust the settings in the configuration section below !!!
// **********************************************************************************
// Copyright Felix Rusu, LowPowerLab.com
// Library and code by Felix Rusu - felix@lowpowerlab.com
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                             nideid           
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
//#include <LowPower.h> //get library from: https://github.com/lowpowerlab/lowpower

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID     100  //the same on all nodes that talk to each other
#define RECEIVER      1    //unique ID of the gateway/receiver
#define SENDER        2
#define NODEID        RECEIVER  //change to "SENDER" if this is the sender node (the one with the button)
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    //uncomment only for RFM69HW! Remove/comment if you have RFM69W!
//*********************************************************************************************

#define SERIAL_BAUD   115200
#define LED           9 //Moteinos have onboard LEDs on D9
#define BUTTON_INT    1 //user button on interrupt 1 (D3)
#define BUTTON_PIN    3 //user button on interrupt 1 (D3)
RFM69 radio;
volatile long i = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting");
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.setPowerLevel(31);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(0);
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  Serial.flush();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  attachInterrupt(BUTTON_INT, handleButton, FALLING);
  
  radio.readAllRegs();
}

//******** THIS IS INTERRUPT BASED DEBOUNCING FOR BUTTON ATTACHED TO D3 (INTERRUPT 1)
#define FLAG_INTERRUPT 0x01
volatile unsigned long mainEventFlags = 0;
boolean buttonPressed = false;
void handleButton()
{
  mainEventFlags |= FLAG_INTERRUPT;
}

byte LEDSTATE=LOW; //LOW=0
void loop() {
  //******** THIS IS INTERRUPT BASED DEBOUNCING FOR BUTTON ATTACHED TO D3 (INTERRUPT 1)
  
  if (mainEventFlags & FLAG_INTERRUPT)
  {
    //LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_ON);
    mainEventFlags &= ~FLAG_INTERRUPT;
    if (!digitalRead(BUTTON_PIN)) {
      buttonPressed=true;
    }
  }
  i++;
  
  if (i > 0x2FFFE && digitalRead(BUTTON_PIN))
  {
    i = 0;
    //buttonPressed = true;
  }
  
  if (buttonPressed)
  {
    Serial.println("Button pressed!");
    buttonPressed = false;
    radio.send(RECEIVER, "Hi", 2, false); //target node Id, message as string or byte array, message length
      //Blink(LED, 40, 3); //blink LED 3 times, 40ms between blinks
    digitalWrite(LED, !digitalRead(LED));
  }
  
  //check if something was received (could be an interrupt from the radio)
  if (radio.receiveDone())
  {
    Serial.println("Receive got");
    Serial.println(radio.DATALEN);
    Serial.println(radio.DATA[0]);
    Serial.println(radio.DATA[1]);
    Serial.println(radio.DATA[2]);
    
    //check if received message is 2 bytes long, and check if the message is specifically "Hi"
    if (radio.DATALEN==2 && radio.DATA[0]=='H' && radio.DATA[1]=='i')
    {
      Serial.println("Packet got");
      
      // Auto respond?
      if (!digitalRead(BUTTON_PIN))
      {
        buttonPressed = true;
      }
      
      digitalWrite(LED, !digitalRead(LED));
      Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.println("]");
    }
   
    //check if sender wanted an ACK
    if (radio.ACKRequested())
    {
      radio.sendACK();
      Serial.println(" - ACK sent");
      //digitalWrite(LED, !digitalRead(LED));
    }
    else
    {
      radio.send(RECEIVER, "Hi", 2, false);
      Serial.println("Response sent");
    }
  }
  
  radio.receiveDone(); //put radio in RX mode
  Serial.flush(); //make sure all serial data is clocked out before sleeping the MCU
  //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); //sleep Moteino in low power mode (to save battery)
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i=0; i<loops; i++)
  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}
