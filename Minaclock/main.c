//import libraries
#include <stdint.h>
#include <stdbool.h>
#include "MK64F12.h"
//PDOR = Port data output
//PSOR = Port set output register
//PCOR = Port clear output registor
//PTOR = Port toggle output register
volatile uint32_t msTicks = 0; //For systick
#define MASK(x) (1UL << (x)) //Set up bit via masks
volatile int stop_mode = 0;
void SysTick_Handler(void) {
   msTicks++;
}
volatile uint32_t lastButtonTick = 0;
#define DEBOUNCE_TIME 100U
 
static const uint8_t SEGMENT_MAP[10] = {
   0xC0, //1
   0xF9, //2
   0xA4, //3
   0xB0, //4
   0x99, //5
   0x92, //6
   0x82, //7
   0xF8, //8
   0x80, //9
   0x90 //0
};
static const uint8_t SEGMENT_SELECT[4] = { 0xF1, 0xF2, 0xF4, 0xF8 };
void initHardware(void) {
    //Open clock
   SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK;
   //Set up buzzer
   PORTA->PCR[1] = PORT_PCR_MUX(1);
	PTA->PDDR &= ~MASK(1);  // Input = high impedance
   //Suspected to be unnecessary
   PORTB->PCR[22] = PORT_PCR_MUX(1);
   PTB->PSOR = (1U << 22);
   PTB->PDDR |= (1U << 22);
   //Button for changing status
   PORTB->PCR[3] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
   PTB->PDDR &= ~(1U << 3);
   //7-segment latch
   PORTB->PCR[23] = PORT_PCR_MUX(1);
   PTB->PDDR |= (1U << 23);
   //7 segment clock
   PORTC->PCR[3] = PORT_PCR_MUX(1);
   PTC->PDDR |= (1U << 3);
    //Write for 7-segment
   PORTC->PCR[12] = PORT_PCR_MUX(1);
   PTC->PDDR |= (1U << 12);
    //LED pin
   PORTD->PCR[0] = PORT_PCR_MUX(1);
   PTD->PDDR |= (1U << 0);
   PTD->PDOR |= (1U << 0);
   SystemCoreClockUpdate();
   SysTick_Config(SystemCoreClock / 1000U);
}
static void shortDelay(void) {
   for (volatile int i = 0; i < 20; i++) {
       __NOP(); //Delay using NOP
   }
}
static void shiftOutByte(uint8_t data) {
   for (int i = 7; i >= 0; i--) {
       if (data & (1U << i)) { //Set and clear bits in each display value
           PTC->PSOR = (1U << 12);
       } else {
           PTC->PCOR = (1U << 12);
       }
       shortDelay(); //Blink for clock refreshing
       PTC->PSOR = (1U << 3);
       shortDelay();
       PTC->PCOR = (1U << 3);
   }
}
static void writeDigit(uint8_t digitPos, uint8_t segCode) {
   PTB->PCOR = (1U << 23); //set latch to 0
   shiftOutByte(segCode); //show the number
   shiftOutByte(SEGMENT_SELECT[digitPos]); //show the position
   PTB->PSOR = (1U << 23); //set latch to 1
}
void refreshDisplay(uint16_t number) {
   static uint32_t lastRefresh = 0;
   static uint8_t currentDigit = 0;
   //refresh every 2 ms
   if ((msTicks - lastRefresh) >= 2U) {
       lastRefresh = msTicks;
       uint8_t segs[4];
       segs[0] = (number >= 1000) ? SEGMENT_MAP[(number / 1000) % 10] : 0xFF; //set number to segment map
       segs[1] = (number >= 100)  ? SEGMENT_MAP[(number / 100) % 10]  : 0xFF;
       segs[2] = (number >= 10)   ? SEGMENT_MAP[(number / 10) % 10]   : 0xFF;
       segs[3] = SEGMENT_MAP[number % 10];
       writeDigit(currentDigit, segs[currentDigit]);
       currentDigit = (currentDigit + 1U) % 4U; //rotating every 4 digits
   }
}
void alarmOn(void) {
   // Drive PTA1 LOW to activate buzzer
    PTA->PCOR = MASK(1);
    PTA->PDDR |= MASK(1);
   PTD->PDOR &= ~MASK(0);
}
void alarmOff(void) {
   // Release PTA1 (high impedance)
    PTA->PDDR &= ~MASK(1);
   PTD->PDOR |= MASK(0);
}
bool isButtonPressed(void) {
   return ((PTB->PDIR & (1U << 3)) == 0U); //Check if it's pressed (active-low)
}
void delayWithDisplay(uint32_t delayMs, uint16_t displayNum) {
   uint32_t start = msTicks;
   while ((msTicks - start) < delayMs) {
       refreshDisplay(displayNum);
   }
}
void countSeconds(uint32_t targetSeconds)
{
    uint32_t elapsedMs = 0;
    uint32_t previousTime = msTicks;
    while (elapsedMs < targetSeconds * 1000U)
    {
        uint32_t currentTime = msTicks;
        uint32_t deltaTime = currentTime - previousTime;
        previousTime = currentTime;
        // Only count time when stop_mode is 0
        if (stop_mode == 0)
        {
            elapsedMs += deltaTime;
        }
        // Calculate remaining seconds
        uint32_t remainingSec =
            targetSeconds - (elapsedMs / 1000U);
        refreshDisplay((uint16_t)remainingSec);
    }
}
void alarmUntilButtonPressed(void) {
   alarmOn(); //turn on alarm
   while (!isButtonPressed()) {
       refreshDisplay(0); //refresh display
   }
   delayWithDisplay(30U, 0);
   while (isButtonPressed()) {
       refreshDisplay(0);
   }
   delayWithDisplay(30U, 0);
   alarmOff();
}
int main(void) {
   initHardware(); //initialize hardware
    __disable_irq(); //Reset all IRQs for a new value
    // Configure PTB10 as GPIO input with internal pull-up
    PORTB->PCR[10] = PORT_PCR_MUX(1) //
                   | PORT_PCR_PE_MASK
                   | PORT_PCR_PS_MASK
                   | PORT_PCR_IRQC(0xA);
    PTB->PDDR &= ~MASK(10); //Set PTB10 as INPUT
    // Clear pending interrupt flag
    PORTB->ISFR = MASK(10);
    // Configure PORTB interrupt in NVIC
    NVIC_ClearPendingIRQ(PORTB_IRQn);
    NVIC_EnableIRQ(PORTB_IRQn);
	lastButtonTick = msTicks - DEBOUNCE_TIME;
    //enable interrupt
    __enable_irq();
   while (1) {
       countSeconds(15);
       alarmUntilButtonPressed();
       countSeconds(5);
       alarmUntilButtonPressed();
   }
}
void PORTB_IRQHandler(void)
{
    if (PORTB->ISFR & MASK(10))
    {
        uint32_t currentTime = msTicks;
 
        // Debounce: accept only presses separated by 100 ms
        if ((currentTime - lastButtonTick) >= DEBOUNCE_TIME)
        {
            lastButtonTick = currentTime;
 
            stop_mode = !stop_mode;
        }
 
        // Always clear interrupt flag
        PORTB->ISFR = MASK(10);
    }
}