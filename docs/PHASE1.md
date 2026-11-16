# Fase 1 — contratos portáveis dos adaptadores

Esta etapa liga os módulos do core a interfaces que um backend de dispositivo
pode alimentar, sem incluir SDL2, PSL1GHT ou headers de hardware no core.
Os contratos são C++11 e não abrem dispositivos.

## Input

`PlatformControllerSample` representa estados canônicos de D-pad, botões,
aliases lógicos de pulo e eixos assinados de 16 bits já normalizados. O backend converte a orientação e a faixa
física, aplica sua dead-zone e então chama
`apply_platform_controller_sample()`. A função substitui somente os campos
DualShock 3 de `InputSnapshot`; teclado e regras do jogo continuam com o
chamador. Cross conserva a semântica de confirmação/pulo de `InputMapper`.

## Áudio

`render_audio_callback()` entrega PCM16 estéreo interleaved em blocos de até
`max_chunk_frames`. O chamador fornece o buffer e serializa o callback com as
operações de controle de `AudioManager`. Falha, exceção ou configuração inválida
silencia o buffer quando seu tamanho pode ser calculado. O wrapper não abre SDL2
Audio nem CellAudio e não mede latência real.

## Vídeo

`Framebuffer::pixel_data()` oferece acesso somente para leitura ao armazenamento
CPU. `present_framebuffer()` copia os pixels para uma superfície pertencente ao
chamador, com stride por linha e ordem RGBA8888 ou BGRA8888. A função valida
capacidade, stride e overflow antes de escrever; não aloca, não retém o destino
e não apresenta pixels por SDL2 ou RSX.

## Limites da etapa

Os três adaptadores são contratos host-testáveis. Drivers que leem DualShock 3,
abrem dispositivo de áudio, inicializam vídeo, registram callbacks do sistema,
retornam ao XMB ou carregam arquivos do HDD/USB ainda precisam de dependências
verificadas e validação em host/emulador/hardware. Nenhum backend nativo ou PKG é
declarado pronto por esta etapa.

Veja [QA_PHASE1.md](QA_PHASE1.md) para cenários e estado de execução.

## Backend de host Windows

`Win32RuntimePlatform` implementa `RuntimePlatform` para permitir uma
demonstração interativa do `World` e do `RuntimeLoop` em Windows. A classe cria
uma janela Win32, processa fechamento, captura teclado apenas quando a janela
está em primeiro plano e mede elapsed time com `QueryPerformanceCounter`.
`Framebuffer::pixel_data()` é convertido para BGRA top-down e apresentado por
GDI. O alvo `pc2ps3_pc_demo` é uma demonstração original com física genérica;
não usa assets nem parâmetros alegadamente extraídos de jogos comerciais.

O adapter mapeia setas/WASD, Espaço/Z, J/X, K/C, Q/E, P, Tab, Enter e
Backspace para `InputSnapshot`; a demo consome movimento, pulo e Escape para
encerrar. Testes exercitam mapeamento, ordem/canais de pixels e ciclo de vida de
uma janela oculta. Este adapter não contém áudio de dispositivo nem substitui
backend SDL2_PSL1GHT/RSX, cellPad ou cellAudio. O executável PS3 e o PKG seguem
bloqueados até existir toolchain com procedência e build verificáveis.

## Controle Windows por XInput

`Win32GamepadAdapter` carrega XInput opcionalmente da pasta System32 e mantém a dependência fora do core. `poll()` converte o D-pad, o stick esquerdo, A/B/X/Y, LB/RB, Start/Back e gatilhos no `PlatformControllerSample` existente. Dead-zone do stick usa a constante do XInput; Y é normalizado para positivo para baixo. A e Cross representam pulo; LB ou gatilho esquerdo ≥ 30 representam L1, RB ou gatilho direito ≥ 30 representam R1. A ausência/desconexão do controle retorna status próprio e sample neutro, sem apagar o teclado. A leitura ocorre no thread do chamador e não aloca por quadro. XInput é opcional; este contrato não implementa DirectInput nem valida controle físico conectado.

## Pacing de frames no host Windows

`Win32RuntimePlatform` aceita um teto opcional de 1–240 FPS; zero desativa a espera. O padrão é 60 FPS. O adapter mede com QPC e usa `MsgWaitForMultipleObjectsEx` com timeout arredondado para cima, voltando a processar mensagens antes de cada espera. Assim, fechamento e input da janela interrompem a espera sem mudar globalmente a resolução do timer. `elapsed_ns` inclui o tempo ocioso. Se um frame atrasar, o próximo deadline é recalculado sem catch-up em rajada. O pacing só atua no backend Win32; o scheduler portátil permanece inalterado.

## Saída de áudio WinMM

`Win32AudioOutput` envia PCM16 estéreo na taxa de `AudioManager` em uma fila limitada de 2–8 blocos pré-alocados (64–4096 frames cada). `pump()` recicla somente blocos concluídos e chama `render_audio_callback` no mesmo thread do jogo, conforme o contrato single-threaded do mixer. A callback WinMM só sinaliza um evento; não acessa o mixer. Falhas de renderização deixam o bloco em silêncio. Falhas transitórias de reset/unprepare/close são tentadas novamente; se o driver continuar recusando unprepare ou close, `shutdown()` retorna false e mantém os recursos ainda possivelmente referenciados sob ownership para uma nova tentativa. O chamador deve manter o objeto vivo e repetir `shutdown()` antes de destruí-lo; se o driver nunca liberar os recursos, a segurança exige preservá-los em vez de liberar memória referenciada. A API WinMM e o dispositivo são opcionais; teste sintético não prova reprodução física.

O demo Win32 liga `Win32AudioOutput` ao ciclo de vida da aplicação: inicializa no `start()`, chama `pump()` no thread do loop e tenta `shutdown()` no `stop()`. Falha de dispositivo ou de pump desativa a saída sem encerrar o runtime. O demo não cria sons; o sink envia silêncio até que a aplicação submeta clips ao `AudioManager`.
