# Estado técnico

> **Atualização de 4 nov 2026.** Esta nota substitui as estimativas e os bloqueios abaixo,
> que ficam como histórico. A Fase 0 foi destravada: há toolchain Docker fixada, ELF PPU e
> PKG `PCPS00001`, que dá boot no RPCS3 0.0.43 com o vertical slice jogável. Entregue: o
> personagem animado por atlas local opcional, save, QA automática determinística
> (588 ticks) e CTest 70/70. Detalhes em [ENTREGA.md](ENTREGA.md). Falta: console físico, áudio audível, D-pad
> e o port do Cuphead.


## Estimativa de conclusão

Estimativa informal: **~41% do prompt completo** e **~93% do núcleo portátil da
Fase 2**. Não é uma métrica ponderada por linhas: a plataforma PS3, PKG/RPCS3,
fidelidade do jogo comercial e validação em hardware ainda são grandes entregas
pendentes. O controle do projeto é ClickUp, lista PC2PS3. Vinte e dois cards com
status `CONCLUÍDO` estão no ClickUp: nove históricos, assets, estrutura e gates seguros, três cards
de interpolação visual, três adaptadores da Fase 1, três módulos do runtime e
InputManager.
Cards futuros ficam no backlog local para sincronização em lotes de três. O board
MCP ainda retorna `BOARD_TOKEN_MISSING` (`CLICKUP_PERSONAL_TOKEN`); a sessão web
ClickUp está autenticada. Sete cards sincronizados nesta sessão ainda precisam
dos checklists Dev/QA/DoD: a criação rápida não os incluiu, a API não tem token e
o navegador de automação ficou indisponível.

O contrato lógico do InputManager inclui eixos e ações do DualShock 3. O teste
de conflito `input_mapping` agora passa em Debug após neutralizar qualquer
oposição, independente da contagem de fontes; a suíte host completa passou em
31/31 CTests. Evidências em [QA_PHASE2.md](QA_PHASE2.md).

## Última validação host — 2026-11-02
CMake 3.31.6 e MSVC 19.44.35228.0 foram localizados no Visual Studio Build
Tools. O build Debug e CTest completo passaram em 31/31; isso substitui os
registros anteriores abaixo que diziam que CMake/CTest não estavam disponíveis.
Os testes usam `assert`, então Release com `NDEBUG` não conta como evidência.
Esta validação cobre o host, não SDL2/PSL1GHT, toolchain PPU, PKG, RPCS3 ou PS3.

## Fase 0

Resultado da QA da Tifa: **BLOCK**. Em 2026-11-02, `setup` e `build-ps3` falharam antes do Docker porque falta `docker/toolchain.lock.json`; `package-ps3` bloqueou sem ferramentas SELF/SFO/PKG verificadas. O `clean` padrão permaneceu em dry-run e não removeu arquivos. Ainda não há imagem Docker construída, ELF PPU, `EBOOT.BIN`, `PARAM.SFO` ou PKG real.

## Estrutura

A árvore do prompt é sugerida e permite ajustes conforme implementação. Este checkout usa `include/pc2ps3/` e `src/core/` no lugar de uma pasta `core/` direta, e acrescenta `samples/`. O layout adaptado está mapeado em [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md), com subpastas documentadas para análise, plataformas PC/PS3, jogo original, ferramentas de assets e packaging. `Test-Path` confirmou os caminhos e CMake continua apontando para os scripts atuais. `original-pc/` foi omitida para não armazenar jogos comerciais; packaging é apenas esqueleto e nenhum artefato PS3 foi gerado. Critério da estrutura: **PASS**.

## Assets e procedência

`assets/phase0/` contém apenas `ICON0.PNG` e `PIC1.PNG`, gerados por [make_phase0_art.py](../tools/make_phase0_art.py) com geometria local e documentados como CC0-1.0 em [SOURCE_ORIGIN.md](SOURCE_ORIGIN.md). O inventário do checkout não encontrou EXE, DLL, ELF, SELF, SPRX, PKG, RPX, ISO ou BIN comerciais/protegidos. O critério de assets foi registrado como **PASS**. A licença da base C++ importada continua não declarada, uma pendência de procedência distinta.

## Scanner de conteúdo

A avaliação integral continua marcada como BLOCKED. A verificação por trechos isolou o alerta em `tools/phase0.py` nas linhas 197–199, que validam a opção `--confirm` para limpeza, e em `docs/QA_PHASE0.md` nas linhas 92–93, que registram o dry-run do wrapper e a consulta por CMake. Esses trechos descrevem controles e resultados, sem instrução para ignorar salvaguardas; há indício de falso positivo. O prompt original foi avaliado como CLEAN. Nenhuma mudança foi feita no código de limpeza.

## Próximo bloqueio

Retomar a comprovação de Docker, ELF PPU e PKG quando houver pins/digests verificáveis e um daemon Docker Linux com a toolchain PS3 disponível. Veja [PHASE0.md](PHASE0.md) para os pré-requisitos e [QA_PHASE0.md](QA_PHASE0.md) para o registro da Tifa.

## Fase 2 — runtime portátil

Foi implementado o primeiro recorte de física AABB integrado ao movimento de `World`, um adaptador portátil de input para estados lógicos de teclado/DualShock 3, reprodução temporal de frames de animação e gerenciamento portátil de cenas com transições adiadas. Alvos permanentes cobrem AABB, transições de input, animação e lifecycle de cenas; a suíte host Debug completa passou em 31/31. A colisão AABB é discreta e pode atravessar sólidos muito finos; o player de animação referencia um clip que precisa continuar vivo, `reset()` mantém o clip selecionado, e não há renderer/carregamento de atlas. O gerenciador de cenas é single-threaded e não integra renderer/plataforma. A leitura de hardware pelos backends SDL2/PSL1GHT segue pendente. Veja [PHASE2.md](PHASE2.md) e [QA_PHASE2.md](QA_PHASE2.md).

### SaveManager e filesystem (contrato portátil)
`FileSystem` e `SaveManager` foram adicionados com slots de nome gerado, envelope PC2S v1, length little-endian, CRC-32/IEEE, leitura limitada e publicação da saída somente após validação. `storage_and_saves` passou na suíte CTest Debug. Ainda faltam um backend real para disco/PS3 e evidência de escrita atômica no destino. Consulte [ADR 0003](adr/0003-save-storage.md) e [QA_PHASE2.md](QA_PHASE2.md).

### AudioManager (PCM/WAV e mixer portátil)
`PcmClip`, decoder WAV RIFF PCM 8/16-bit e mixer PCM16 estéreo com até 64 vozes, resampling linear, volumes por categoria, saturação e fonte pull de música foram implementados sem backend de dispositivo. `audio_mixing` passou na suíte CTest Debug; saída real de SDL2/CellAudio, latência, codecs comprimidos e sincronização específica de plataforma permanecem pendentes. Consulte [ADR 0004](adr/0004-audio.md) e [QA_PHASE2.md](QA_PHASE2.md).

### Pipeline de assets
O scanner gera manifesto v2 determinístico com hashes em blocos, categorias por extensão, skips para links/reparse/special files e escrita atômica fora da raiz. Agora aceita mapa JSON v1 para dependências explícitas, validando referências, caminhos, duplicatas, ciclos e diretórios pai sem symlink/reparse point; não infere relações a partir de binários. Não lê nem modifica conteúdo, e não atribui licença/procedência que não foram informadas. Conversão, formatos comerciais e verificação de DRM permanecem fora do escopo. Consulte [ASSET_PIPELINE.md](ASSET_PIPELINE.md).
A suíte isolada Python executou: **16 testes, 13 PASS, 3 SKIP** (criação de symlink indisponível no sandbox Windows). A CLI gerou manifesto válido com dependências ordenadas e rejeitou referência ausente sem saída parcial. CMake/CTest não foi executado por falta de CMake. Consulte [QA_ASSETS.md](QA_ASSETS.md).

### HUD de depuração portátil
O framebuffer agora pode desenhar FPS e bytes de memória fornecidos pelo
chamador, com fonte pequena, formatação limitada e clipping. A coleta de RAM do
processo e a integração dos adaptadores SDL2/PSL1GHT continuam pendentes; não há
alegação de medição real neste módulo. O teste C++ permanente está no alvo
debug_hud, mas ainda depende da disponibilidade de CMake e compilador.

### Texturas RGBA e regiões de atlas
Texture agora possui pixels RGBA já decodificados e o framebuffer compõe regiões
de atlas com alpha e clipping. `decode_png()` agora converte PNG 8-bit não
interlaçado para Texture e valida limites, chunks, CRC e fluxo zlib/DEFLATE.
O backend gráfico de dispositivo continua pendente; CMake/CTest também segue
indisponível neste ambiente. MSVC 14.44 compilou e executou os testes
`image_decoder`, `render_texture`, `animation_render` e `core` em `/std:c++14`;
o código mantém apenas recursos C++11. CMake/CTest não foi configurado porque
CMake não está instalado.

### Tilemaps e câmera
`TileMap` suporta camadas limitadas por depth, índices de atlas, clipping e
scroll em pixels; `tilemap` passou em compilação e execução manual MSVC 14.44
com `/std:c++14`. CMake/CTest continua indisponível neste ambiente.

### Partículas
`ParticleSystem` usa slots pré-alocados, velocidade/lifetime limitados, tempo
inteiro determinístico e clipping/alpha do framebuffer; `particles` passou em
compilação e execução manual MSVC 14.44 (`/std:c++14`). CMake/CTest indisponível.

### Integração animação e renderer
O framebuffer pode desenhar o retângulo do AnimationFrame selecionado usando
uma Texture fornecida pelo chamador, sem avançar o player. O decoder PNG está
disponível; o backend de apresentação continua pendente. MSVC validou os testes
relacionados, mas CMake/CTest não está instalado.

### Demo headless integrado
O demo gera arte original em memória, anima o frame do atlas e renderiza o HUD
em prototype.ppm. MEM informa somente os buffers de pixels da Texture e do
Framebuffer; FPS é a taxa-alvo de simulação, não medição de tempo real. Não
representa RSS ou memória total do processo.

## TextureManager
`TextureManager` liga o `FileSystem` lógico ao decoder PNG, com cache on-demand,
handles por geração, limite de slots/bytes, unload e reaproveitamento de buffers
CPU. Seu teste CLI passou em MSVC 14.44 (`/std:c++14`); CMake/CTest não foi
executado. Consulte [PHASE2.md](PHASE2.md) e [QA_PHASE2.md](QA_PHASE2.md).
Upload de texturas no dispositivo continua pendente.

### JumpAssist
`JumpAssist` integra jump buffer e coyote time de seis ticks ao `World`, com
consumo único e reset determinístico. O teste manual passou em MSVC 14.44
(`/std:c++14`); CMake/CTest e compilação estrita `/std:c++11` continuam
indisponíveis no host.

### TriggerSystem
`TriggerSystem` fornece sensores AABB estáticos e eventos enter/exit determinísticos,
com capacidade limitada e sem alocações em `update()`. A suíte manual passou em
MSVC 14.44 (`/std:c++14`); CMake/CTest e compilação estrita `/std:c++11` continuam
indisponíveis no host. Sweep contínuo permanece futuro.

### StateMachine, CheckpointSystem e ProjectileSystem
Os três módulos e testes permanentes passaram em executáveis host com MSVC 14.44
(`/std:c++14`). O build integrado do demo e o smoke test também passaram. CMake,
CTest e compilação estrita `/std:c++11` não puderam ser executados neste host; o
build completo ainda mostra o warning preexistente C4244 em `audio.cpp:299`.
O registro local e a evidência estão em `LOCAL_BACKLOG.md` (nota local, fora do Git) e
[QA_PHASE2.md](QA_PHASE2.md), aguardando sincronização ao ClickUp quando `cup auth`
voltar a funcionar. O ClickUp permanece como sistema de gestão do projeto.

### Plataformas móveis, hitbox/hurtbox e knockback
Os três módulos passaram em testes focados com MSVC 14.44 (`/std:c++14`), e a
regressão `core_determinism` foi recompilada e executada após a integração das
plataformas e do knockback. O build integrado completo e o smoke test do demo
também passaram. CMake/CTest e `/std:c++11` estrito continuam indisponíveis; o
build integrado mostra apenas o warning preexistente C4244 em `audio.cpp:299`.
Os cenários e o estado dos cards locais estão em [QA_PHASE2.md](QA_PHASE2.md) e
`LOCAL_BACKLOG.md` (nota local, fora do Git). Vinte e dois cards concluídos estão
sincronizados via Playwright; a API/CLI ClickUp ainda não autentica e cards
futuros permanecem no backlog local.

## Fase 1 — adaptadores portáveis
Foram adicionados contratos host-testáveis para input canônico, callback PCM16
com fallback silencioso e cópia de framebuffer RGBA/BGRA com stride. Os três
alvos passaram dentro da suíte CTest Debug completa (31/31) com MSVC 19.44.
Os cards foram sincronizados via Playwright e estão concluídos na lista PC2PS3:
input [86e3xxj1f](https://app.clickup.com/t/90171191125/86e3xxj1f), áudio
[86e3xxkgg](https://app.clickup.com/t/90171191125/86e3xxkgg) e framebuffer
[86e3xxkk4](https://app.clickup.com/t/90171191125/86e3xxkk4). SDL2, PSL1GHT,
vídeo RSX, callbacks do sistema PS3 e integração nativa continuam pendentes.
Veja [PHASE1.md](PHASE1.md) e [QA_PHASE1.md](QA_PHASE1.md).
## Lote concluído — runtime loop (2026-11-02)
Scheduler de passo fixo, distribuição de input por tick e RuntimeLoop portátil implementados. Build CMake Debug e suíte host passaram em 34/34 CTests; QA e critérios estão em [QA_PHASE2.md](QA_PHASE2.md), backlog local em `LOCAL_BACKLOG.md` (nota local, fora do Git) e arquitetura em [ADR 0010](adr/0010-runtime-loop.md). Os cards foram sincronizados e concluídos via Playwright: scheduler [86e3xxm3f](https://app.clickup.com/t/90171191125/86e3xxm3f), distribuidor [86e3xxm5y](https://app.clickup.com/t/90171191125/86e3xxm5y) e RuntimeLoop [86e3xxm7b](https://app.clickup.com/t/90171191125/86e3xxm7b). MSVC 19.44 não oferece modo estrito `/std:c++11`; backend SDL2/PSL1GHT, toolchain e hardware PS3 continuam sem validação.
## Lote concluído — interpolação visual (2026-11-02)
RuntimeLoop passa alpha ao render; World fornece posição interpolada render-only; o demo headless compõe runtime, input scripted e World e gera PPM/JSON determinísticos. Build CMake Debug e CTest passaram em 36/36; cenários/limites em [QA_PHASE2.md](QA_PHASE2.md), cards em `LOCAL_BACKLOG.md` (nota local, fora do Git), arquitetura em [ADR 0011](adr/0011-render-interpolation.md). Os três cards foram recriados via Playwright e marcados concluídos na lista PC2PS3; o board MCP ainda está sem token. MSVC 19.44 não oferece modo estrito `/std:c++11`, e nenhum backend/toolchain/hardware PS3 foi validado.

## Entrega local — HostFileSystem (2026-11-02)

Implementado o adaptador Windows de disco para a interface FileSystem, com raiz confiável, paths lógicos relativos, leitura limitada, escrita temporária seguida de flush/substituição e remoção de arquivos regulares. Build Debug sem warnings; suíte host CTest Debug 37/37. A criação de symlink, hard link e junction foi bloqueada pelo sandbox Windows e os cenários foram SKIP; consulte QA_PHASE2.md. O card está no backlog local, não no ClickUp: board MCP continua sem CLICKUP_PERSONAL_TOKEN. Esta entrega não valida SDL2/PSL1GHT, PPU, RSX, PKG, RPCS3 nem hardware PS3.

## Integração de disco — lote de três cards (2026-11-02)

HostFileSystem, persistência de SaveManager e carregamento de TextureManager foram integrados no host Windows. CMake Debug concluiu sem warnings; CTest completo passou em 39/39. Os relatórios dos cenários Tifa estão em QA_PHASE2.md e os três cards locais em LOCAL_BACKLOG.md. Os cenários de links seguem SKIP por restrição do sandbox Windows; o backend nativo PS3 e SDL2 ainda não foram validados. ClickUp permanece pendente por BOARD_TOKEN_MISSING.

## Lote local concluido   texto, overlay e frame time (2026-11-02)

Tres cards da Fase 2 foram implementados e registrados localmente. CMake
Debug/CTest passou em 4/4 (`debug_hud`, `render_text`, `vfx_overlay`,
`frame_time`). Cen�rios e limites: [QA_PHASE2.md](QA_PHASE2.md); backlog:
`LOCAL_BACKLOG.md` (nota local, fora do Git). ClickUp/Board aguarda
`CLICKUP_PERSONAL_TOKEN`; backend SDL2/PSL1GHT/RSX e PS3 seguem sem validacao.
