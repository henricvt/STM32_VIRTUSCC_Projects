# Projeto Final Bare Metal - STM32

Este repositório contém o código-fonte e a documentação do projeto final desenvolvido para a **Formação em Sistemas Embarcados Virtus-CC**, no módulo de *Introdução aos Microcontroladores de 32 bits - Bare Metal*.

## 🎯 Motivação e Objetivos

O monitoramento de condições em máquinas girantes é essencial para prevenir falhas inesperadas em equipamentos críticos, evitando paradas não planejadas e prejuízos operacionais. A manutenção preditiva permite identificar sinais de defeito antes que se tornem falhas catastróficas. 

O objetivo principal deste projeto é desenvolver uma aplicação inteligente (Edge Computing) em arquitetura *Bare Metal* capaz de identificar falhas em motores através da análise de sinais de vibração. O sistema foi projetado sob uma rígida métrica de otimização energética para garantir a operação contínua por cerca de 10 anos, alimentado por uma bateria Li-SOCL2 de 3400 mAh.

---

## 🛠️ Descrição da Solução e Funcionalidades

O firmware foi desenvolvido em linguagem C e utiliza um microcontrolador STM32 em conjunto com um acelerômetro digital (MPU6050) para coletar dados de vibração no eixo Z. A solução atende a todos os requisitos propostos:

### Requisitos Mínimos Atendidos
* **Frequência de Aquisição:** O timer (TIM2) foi configurado para gerar interrupções a 1000 Hz, superando o requisito mínimo de 250 Hz para captura do sinal de aceleração.
* **Monitoramento Periódico:** O sistema realiza uma coleta de dados por turno, sendo acordado a cada 8 horas por um alarme do RTC.
* **Análise de Desbalanceamento:** Cálculo do valor RMS do sinal de vibração (após remoção do componente DC) para identificar problemas de desbalanceamento.
* **Indicadores de Alerta:** Utilização de GPIOs para acionar um LED e um Buzzer (indicador luminoso e sonoro) caso os níveis de vibração ultrapassem o limite crítico.
* **Rotina de Calibração:** Implementação de uma rotina de calibração inicial. O status da calibração é salvo no registrador de backup do RTC (`RTC_BKP_DR0`) para evitar recalibrações desnecessárias após um reset.

### Requisitos Avançados Implementados
* **Interface com Usuário:** Integração de uma tela OLED (SSD1306) via protocolo I2C para exibição detalhada dos diagnósticos locais, mantendo estratégias de baixo consumo.
* **Processamento no Domínio da Frequência:** Aplicação de Transformada Rápida de Fourier (FFT de 2048 pontos) utilizando a biblioteca otimizada CMSIS-DSP (`arm_rfft_fast_f32`).
* **Diagnóstico Múltiplo de Falhas:** Além do desbalanceamento, a análise dos picos de magnitude da FFT permite classificar problemas mais complexos, como **folga mecânica** e falhas de **lubrificação**.
* **Histórico e Estatística Local:** O sistema grava um log persistente (`LogEntry_t`) na memória Flash interna do STM32, calculando médias móveis com base nos dados dos últimos 30 dias para uma estimativa preditiva mais robusta.

---

## ⚙️ Arquitetura e Diagrama de Estados

O firmware foi arquitetado como uma Máquina de Estados focada em *Low-Power*, garantindo que o processador permaneça a maior parte do tempo em *Standby Mode*:

1. **Power ON / Reset:** Inicialização dos clocks e periféricos básicos.
2. **Checagem de Backup:** Verifica se a calibração já foi realizada (leitura do RTC Backup Register).
3. **Calibração (se necessário):** O microcontrolador lê os dados do eixo Z, calcula o RMS de base e define os limiares de alarme.
4. **Aquisição de Dados:** O TIM2 é ativado a 1 kHz, coletando 2048 amostras do acelerômetro via I2C para um buffer.
5. **Processamento Digital (DSP):** - Remoção da componente DC.
   - Cálculo do RMS atual.
   - Execução da FFT e busca dos 3 maiores picos de amplitude/frequência.
6. **Diagnóstico e Armazenamento:** Compara os resultados atuais com o histórico salvo na memória Flash (últimos 30 dias). Diagnostica a falha (OK, Desbalanceamento, Folga ou Lubrificação) e salva o novo log na Flash.
7. **Decisão de Estado:**
   - Se o motor estiver `FAULT_OK`: Agenda o próximo alarme para daqui a 8 horas e entra em *Standby*.
   - Se houver falha: Aciona o display OLED com os dados, pisca o LED e toca o Buzzer em loop infinito.

---

## 🧪 Roteiro de Testes Realizados

Conforme o roteiro de testes do projeto, as seguintes etapas foram validadas:
1. **Consumo Teórico e Real:** Cálculo do consumo teórico com base no *datasheet* e extrapolação das medições com multímetro para garantir o limite de autonomia de 10 anos.
2. **Calibração Base:** Utilização do shaker do laboratório para calibrar o sistema em um ponto de operação saudável (sem defeitos).
3. **Injeção de Falhas:** Geração de sinais de vibração anômalos para verificar a eficácia do sistema na identificação de comportamentos inesperados.

---

## 📎 Entregáveis do Projeto

- [x] **Código-Fonte Comentado:** Disponível no arquivo `main.c` neste repositório.
- [x] **Descrição e Arquitetura:** Documentada neste README.
