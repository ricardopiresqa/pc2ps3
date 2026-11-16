# Backend host

`Win32RuntimePlatform` implementa `RuntimePlatform` com janela Win32, teclado,
relógio de alta resolução e apresentação GDI do framebuffer CPU. O alvo
`pc2ps3_pc_demo` exercita o mesmo `World` e `RuntimeLoop` usados pelos testes.

Mapeamento do adapter: setas/WASD movem; Espaço/Z pula; J/X ataca; K/C usa
habilidade; Q/E representam L1/R1; P pausa; Tab abre menu; Enter confirma;
Backspace cancela. A demo consome apenas movimento, pulo e Escape para encerrar;
os demais campos lógicos ficam disponíveis a futuros adapters de jogo. Ela usa
física genérica e não representa parâmetros medidos de jogos comerciais.

O backend é host Windows, não depende de SDL2 e não altera o core. Não fornece
áudio de dispositivo e não comprova SDL2_PSL1GHT, cellPad, cellAudio, RSX,
retorno ao XMB ou execução PS3. O backend PS3 continua separado e depende de
uma toolchain verificada.
