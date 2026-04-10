# Gerador de Senoide via PWM e UART - STM32

Este repositório contém a implementação de um gerador de sinais senoidais desenvolvido para a placa STM32 **Nucleo-F767ZI**. O sistema utiliza modulação por largura de pulso (**PWM**) filtrada por um circuito **passa-baixas RC** para reconstruir a onda analógica fundamental.

##

### Hardware e Filtragem Analógica
Para transformar o sinal digital PWM em uma senóide limpa, foi projetado um filtro passa-baixa RC com o objetivo de atingir uma frequência de corte ($f_c$) que tornasse viável a implementação da senóide com frequência igual a **60 Hz**.

* **Resistor ($R$):** 10 kΩ
* **Capacitor ($C$):** 100 nF
* **Frequência de Corte Alcançada:** 159,15 Hz

A fórmula utilizada para o cálculo da frequência de corte é:

$$f_c = \frac{1}{2\pi \cdot R \cdot C}$$

### Cálculo do Sinal Senoidal via PWM
O valor do *duty cycle* varia senoidalmente para que a média da tensão acompanhe a forma de onda desejada. A fórmula exata implementada no código é:

$$seno[i] = \left( \sin\left(i \cdot \frac{2\pi}{amostras}\right) + 1 \right) \cdot \frac{estouro}{2}$$

**Onde:**
* **`seno[i]`**: Valor de comparação do Timer (*Duty Cycle*).
* **`+ 1`**: Deslocamento vertical (*Level Shift*) para que o sinal seja apenas positivo.
* **`estouro / 2`**: Fator de escala baseado na resolução máxima do Timer.
* **`2π / amostras`**: Passo angular para completar um ciclo completo.

## Tecnologias e Funcionalidades
* **Hardware:** Microcontrolador STM32 (Placa Nucleo-F767ZI).
* **Frequência Ajustável:** Controle em tempo real via comandos **UART**.
* **Baixo THD:** Otimização para redução da Distorção Harmônica Total.
* **Interface de Saída:** Extração da componente fundamental via filtro passivo.
