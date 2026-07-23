// Fake IMXRT1062 register environment for the XLOC2 emulator.
//
// Registers are plain host memory (per-name statics via xemu::fake_reg), so
// firmware reads/writes are harmless. The one register with real side effects
// is LPSPI4_TDR: the DAC8568 sits on LPSPI4, so writes are decoded into
// virtual CV output values (see xemu::dac8568_spi_word).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace xemu {
// Returns a stable reference for a named fake register.
uint32_t &fake_reg(const char *name);

// Write-decoding proxy for LPSPI4_TDR.
struct Lpspi4TdrProxy {
  void operator=(uint32_t w);
  operator uint32_t() const { return 0; }
};
extern Lpspi4TdrProxy lpspi4_tdr_proxy;
}  // namespace xemu

#define XEMU_REG(name) (::xemu::fake_reg(#name))

// --- LPSPI4 (DAC8568) ---
#define LPSPI4_TDR (::xemu::lpspi4_tdr_proxy)
#define LPSPI4_TCR XEMU_REG(LPSPI4_TCR)
#define LPSPI4_SR XEMU_REG(LPSPI4_SR)
#define LPSPI4_CR XEMU_REG(LPSPI4_CR)
#define LPSPI4_CFGR1 XEMU_REG(LPSPI4_CFGR1)
#define LPSPI4_CCR XEMU_REG(LPSPI4_CCR)
#define LPSPI4_FCR XEMU_REG(LPSPI4_FCR)
#define LPSPI4_DER XEMU_REG(LPSPI4_DER)
#define LPSPI4_RDR XEMU_REG(LPSPI4_RDR)
#define LPSPI4_RSR XEMU_REG(LPSPI4_RSR)

// --- LPSPI3 (OLED on SPI1) — display driver is replaced, but keep valid ---
#define LPSPI3_TDR XEMU_REG(LPSPI3_TDR)
#define LPSPI3_TCR XEMU_REG(LPSPI3_TCR)
#define LPSPI3_SR XEMU_REG(LPSPI3_SR)

// --- LPSPI bitfield helpers (encodings don't matter, keep shape) ---
#define LPSPI_TCR_FRAMESZ(n) ((uint32_t)(n)&0xFFF)
#define LPSPI_TCR_PCS(n) (((uint32_t)(n)&3) << 24)
#define LPSPI_TCR_RXMSK ((uint32_t)1 << 19)
#define LPSPI_TCR_CONT ((uint32_t)1 << 21)
#define LPSPI_TCR_CONTC ((uint32_t)1 << 20)
#define LPSPI_SR_TCF ((uint32_t)1 << 10)
#define LPSPI_SR_FCF ((uint32_t)1 << 9)
#define LPSPI_SR_WCF ((uint32_t)1 << 8)
#define LPSPI_SR_MBF ((uint32_t)1 << 24)

// --- IOMUXC pad/mux controls (inert) ---
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01)
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_B0_01 XEMU_REG(IOMUXC_SW_PAD_CTL_PAD_GPIO_B0_01)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 XEMU_REG(IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01)

// --- Misc peripheral types referenced by headers (drivers replaced) ---
typedef int IRQ_NUMBER_t;
typedef struct { volatile uint32_t dummy[128]; } IMXRT_FLEXPWM_t;
typedef struct { volatile uint32_t dummy[64]; } IMXRT_TMR_t;

// --- GPIO port struct (used by OC_digital_inputs.h declarations) ---
typedef struct {
  volatile uint32_t DR, GDIR, PSR, ICR1, ICR2, IMR, ISR, EDGE_SEL;
  uint32_t pad[25];
  volatile uint32_t DR_SET, DR_CLEAR, DR_TOGGLE;
} IMXRT_GPIO_t;

namespace xemu {
IMXRT_GPIO_t *fake_gpio_port(int idx);
}
#define digitalPinToPortReg(pin) (::xemu::fake_gpio_port((pin) >> 5))
#define digitalPinToBitMask(pin) (1u << ((pin)&31))

#endif  // __cplusplus
