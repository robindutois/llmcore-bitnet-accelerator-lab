#include "xil_io.h"
#include "xparameters.h"

#define GPIO_BASEADDR   XPAR_XGPIO_0_BASEADDR
#define GPIO_DATA_REG   0x00
#define GPIO_TRI_REG    0x04

int main(void) {
    Xil_Out32(GPIO_BASEADDR + GPIO_TRI_REG,  0x00000000U);
    Xil_Out32(GPIO_BASEADDR + GPIO_DATA_REG, 0x000000FFU);
    while (1);
    return 0;
}
