# ADR 0004 — Mixer PCM e fronteira de streaming
- Status: decisão de arquitetura para implementação; validação pendente.
- Escopo: TaskSpec `pc2ps3-phase2-audio`.

## Contexto
O runtime precisa misturar efeitos simultâneos e música sem depender de SDL2 ou CellAudio no núcleo. Um callback de áudio precisa ter trabalho e memória previsíveis; codecs comprimidos e dispositivo pertencem aos adaptadores.

## Decisões
1. `PcmClip` mantém PCM16 interleaved, mono ou estéreo, com taxa de amostragem validada e máximo de 16 MiB por clip decodificado. O decoder desta fase aceita WAV RIFF PCM inteiro de 8 ou 16 bits, 1 ou 2 canais e rejeita arquivos acima de 32 MiB antes de percorrer chunks.
2. `AudioManager` produz PCM16 estéreo na taxa configurada e usa no máximo 64 slots de voz alocados na construção. Efeitos retêm `shared_ptr<const PcmClip>`, misturam com resampling linear e saturação determinística.
3. Volumes master, de categoria e de voz ficam em `[0,1]`. Categorias distinguem música, efeitos, UI e voz; efeitos podem ser parados por handle ou categoria.
4. Música é uma fonte `PcmStream` pull, que entrega PCM16 estéreo já convertido para a taxa de saída. O contrato exige leituras sem bloqueio e sem alocação no callback; o manager mistura a fonte com efeitos e suporta parada/repetição.
5. `AudioManager` não é copiável nem movível; ele e `PcmStream` não são thread-safe. O adaptador deve serializar chamadas ou sincronizar o acesso. O callback recebe o buffer do chamador; o mixer não aloca e adia a liberação da última referência de fonte/clip até uma chamada de controle fora de `render`.
6. O núcleo não inicializa dispositivo nem converte codec comprimido. SDL2/CellAudio, buffering de dispositivo, latência real, interrupções de hardware e codecs ficam nos backends.

## Alternativas consideradas
- Dependência direta em SDL2/CellAudio: acopla runtime comum a uma plataforma.
- Decodificar música inteira para memória: eleva uso de RAM e impede streaming.
- Fazer leitura bloqueante/alocação dentro do callback: pode causar estalos e latência.
- Converter qualquer codec no núcleo: adiciona dependências e formatos sem toolchain/decoders verificados.

## Consequências
A mistura e WAV são testáveis deterministicamente em host, mas não comprovam saída audível no PS3. A fonte de música assume PCM já convertido e não decodifica em tempo real.

## Evidências necessárias
- Testes C++11 para WAV válido/malformado, limite, volume, vozes, resampling, saturação, parada, stream, loop e fonte vazia.
- Execução do CTest, revisão Argus e conferência VERITAS; qualquer backend de dispositivo exige QA separado.

## Rollback
Remover decoder/mixer, alvo de teste e documentação desta TaskSpec; módulos anteriores permanecem independentes.
