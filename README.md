# PC2PS3

Framework incremental para estudar e implementar ports nativos de jogos 2D no PlayStation 3. O repositório separa runtime, plataforma, análise, ferramentas e empacotamento. Arquivos comerciais do usuário ficam fora do Git.

## Estado atual

**Base host (CTest 70/70 no contêiner Linux, 4 nov 2026):**

- biblioteca C++11 de simulação determinística;
- timestep fixo de 60 Hz;
- aceleração, limite de velocidade, gravidade, salto e colisão de chão;
- demo headless com telemetria JSON;
- teste de determinismo via CTest;
- inventário local de arquivos com SHA-256;
- frontend de comandos e sample PPU isolado para a Fase 0.

**PS3 — entregue:** o PKG `PCPS00001` (`dist/ps3/pc2ps3.pkg` para RPCS3 e `pc2ps3.gnpdrm.pkg`
para HEN) contém um vertical slice nativo jogável. No RPCS3 foram comprovados boot, vídeo, input,
save e saída limpa. Com um atlas animado gerado localmente (`tools/build_atlas.py`), o jogador usa esses frames.
Guia de instalação, controles, QA e o que falta: **[docs/ENTREGA.md](docs/ENTREGA.md)**.

## Começando com um agente de IA

Para instalar dependências e a stack (Python, Docker, toolchain PS3, CMake) com um agente de código,
cole o prompt de **[docs/AI_SETUP_PROMPT.md](docs/AI_SETUP_PROMPT.md)** no seu agente.

## Requisitos

- Git, Python 3.8+ (só biblioteca padrão) e Docker (no Windows, Docker Desktop com engine Linux/WSL2).
- Opcional: CMake 3.16+, Ninja e compilador C++11 para build nativo do host; RPCS3 para rodar o PKG.

## Build do host

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/pc2ps3_demo
python -m unittest discover -s tests -p "test_*.py"
```

O resultado esperado do demo é uma linha JSON com `tick:180` e o jogador de volta ao chão. A simulação não usa conteúdo comercial.

## Inventário local

Aponte para uma instalação que você está autorizado a analisar. Não copie a pasta para o repositório:

```bash
python3 tools/asset_manifest.py /caminho/para/instalacao -o /tmp/assets.json
```

O utilitário apenas lista tamanho e SHA-256; não descriptografa, remove DRM ou publica arquivos.

## Build PS3

Consulte [docs/PHASE0.md](docs/PHASE0.md) para versões fixadas, saídas e teste no RPCS3. O primeiro `setup` constrói a imagem da toolchain (~30–60 min).

```bash
bash scripts/setup.sh
bash scripts/build-ps3.sh
bash scripts/package-ps3.sh
```

## Organização

```text
include/pc2ps3/   API do runtime
src/core/         simulação independente da plataforma
apps/headless/    demo executável sem assets comerciais
tests/            regressões determinísticas
tools/            inventário e conversores locais
docker/           ambiente reprodutível do alvo PS3
analysis/         evidências locais, fora do Git quando sensíveis
games/            adapters por título, ainda não ativados
platform/         backends PC e PS3, em implementação incremental
```

## Próximo marco

1. Testar o PKG num PS3 físico com HEN, incluindo D-pad e áudio audível.
2. Cuphead: evoluir o módulo local (`games/cuphead/`, fora do Git) sobre as primitivas da Fase 6.
3. Medir traces do original e comparar com a fidelidade do port.

O projeto não tenta transportar a Unity inteira nem tratar EXE como conversor automático para PKG.

No Windows, use os equivalentes `scripts/*.ps1` com Python 3 e Docker Desktop no modo de contêineres Linux. O Linux da toolchain roda dentro dos contêineres.

## Conteúdo comercial e aviso legal

Este repositório contém apenas código e arte autoral. Nenhum jogo, asset, executável, SDK oficial,
chave ou firmware é distribuído. Ports de títulos comerciais (ex.: `games/cuphead/`) e cópias de
instalação ficam locais e no `.gitignore`; use somente jogos que você possui e está autorizado a analisar.
