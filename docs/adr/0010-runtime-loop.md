# ADR 0010 — Agenda fixa e loop portátil de runtime

- Data: 2026-11-02
- Status: aceita para implementação local
- TaskSpecs: `pc2ps3-phase2-fixed-step-scheduler`, `pc2ps3-phase2-input-tick-distributor`, `pc2ps3-phase2-runtime-loop`

## Contexto

O `World` recebe atualizações discretas do chamador e os adaptadores da Fase 1 já expõem amostras lógicas de input e cópia de framebuffer. Ainda falta um ciclo reutilizável que converta tempo de frame em updates fixos, preserve bordas de input entre frames sem tick e defina a ordem de inicialização, polling, atualização, renderização e encerramento. O ambiente não tem toolchain PS3 comprovada.

## Decisão

1. `FixedStepScheduler` fica em `include/pc2ps3/` e `src/core/`. Recebe deltas inteiros em nanossegundos, não lê relógio, limita frames e updates de catch-up, descarta ticks integrais que excedam o limite e conserva apenas a fração final para interpolação.
2. O scheduler calcula quociente e resto sem somar durações que possam exceder `uint64_t`. Configuração inválida retorna resultado inválido sem alterar o acumulador.
3. `InputTickDistributor` também fica no core. Cada amostra substitui held/eixos, enquanto flags `pressed` são acumuladas por OR até o próximo tick; consumos seguintes do mesmo catch-up recebem as flags como false.
4. `RuntimeLoop` e seus contratos abstratos `RuntimePlatform` e `RuntimeApplication` ficam no core. A plataforma fornece cada `InputSnapshot` e o elapsed monotônico já medido pelo backend; o loop não depende de SDL2, PSL1GHT, RSX, relógio ou dispositivo.
5. O chamador mantém ownership de `RuntimePlatform`, `RuntimeApplication` e `Framebuffer`; o runtime usa referências emprestadas. Para cada frame, o loop submete input, executa até o limite de updates, renderiza uma vez mesmo sem tick e solicita uma apresentação.
6. Inicialização ocorre na ordem plataforma → aplicação. O encerramento ocorre aplicação → plataforma, cada callback somente se a inicialização correspondente retornou sucesso. Um callback que falhe retorna status explícito; o cleanup ainda tenta liberar os demais recursos iniciados.
7. Se `RuntimePlatform::initialize()` retornar falha ou lançar, o próprio backend deve desfazer qualquer inicialização parcial, pois `shutdown()` só é chamado após sucesso. Se `RuntimeApplication::start()` falhar ou lançar, ela própria desfaz sua inicialização parcial, pois `stop()` só é chamado após sucesso.
8. A interface é single-threaded. Ela não implementa pacing, leitura física, áudio, janela ou backend PS3.

## Alternativas descartadas

- Colocar polling ou relógio dentro do scheduler: misturaria o core determinístico com API de plataforma e dificultaria testes reproduzíveis.
- Reutilizar diretamente o mesmo `InputFrame` em cada update de catch-up: repetiria ações de borda; descartá-las quando não há tick perderia toques curtos.
- Fazer o runtime possuir ou alocar a superfície gráfica: alteraria o ownership existente do `Framebuffer` e acrescentaria uma política de memória desnecessária.
- Acoplar o loop a SDL2/PSL1GHT antes de uma toolchain verificável: declararia suporte não demonstrado.

## Consequências e rollback

Os três componentes podem ser testados com relógio e plataforma falsos, sem hardware nem dependências externas. O runtime exige que callbacks comuniquem falhas por retorno e que o host meça elapsed fora do core. Rollback: remover apenas os três módulos e testes, suas entradas CMake e as seções específicas desta decisão/documentação, mantendo os sistemas core já existentes.
