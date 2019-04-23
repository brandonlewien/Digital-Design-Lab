#include "cryotimer.h"


void cryotimer_setup(void){

	CRYOTIMER_Init_TypeDef CRYO_Init_Struct;

	CRYO_Init_Struct.debugRun  = CRYO_DEBUG_DISABLE;
	CRYO_Init_Struct.em4Wakeup = CRYO_EM4_WAKEUP;
	CRYO_Init_Struct.enable    = CRYO_DISABLE;
	CRYO_Init_Struct.osc       = cryotimerOscULFRCO;
	CRYO_Init_Struct.period    = cryotimerPeriod_256;
	CRYO_Init_Struct.presc     = cryotimerPresc_128;
	// ^Period of cryotimer wakeup events = ((2^presc)*(2^period))/cryo_freq = ((128)*(256))/32768 = 1sec :)

	CRYOTIMER_Init(&CRYO_Init_Struct);
	CRYOTIMER_Enable(CRYO_ENABLE);
}

void CRYOTIMER_Interrupt_Enable(void){
	LEUART0->IEN = 0;
	LEUART0->IEN = CRYOTIMER_IEN_PERIOD;
	NVIC_EnableIRQ(CRYOTIMER_IRQn);
}


void CRYOTIMER_IRQHandler(void) {
	uint32_t status;
	status = CRYOTIMER->IF & CRYOTIMER->IEN;
	if(status & CRYOTIMER_IF_PERIOD) {

	}

}
