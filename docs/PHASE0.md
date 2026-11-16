# Fase 0 — ambiente mínimo, ainda sem aceite completo

TaskSpec: `pc2ps3-phase0-ambiente`; ClickUp: `86e3xtgxh`.
Decisão: [ADR 0001](adr/0001-fase0.md).

## Pré-requisitos

- Windows: Python 3.8+ como `python`, PowerShell e Docker Desktop (engine Linux/WSL2).
  Linux: Python 3.8+ como `python3`, Bash e Docker.
- CMake no host é opcional: sem ele, `test` roda a suíte com o cmake/g++ da imagem.
- Rede só no `setup` (clona as fontes fixadas). Build e empacotamento rodam com `--network none`.
  Nenhum SDK Sony, chave, firmware ou conteúdo comercial é usado.

## Toolchain fixada (comprovada nesta máquina em 2026-11-04)

Receita: `docker/ps3dev.Dockerfile`, reproduzida do port
[akinozgen/fallout1-ce-ps3](https://github.com/akinozgen/fallout1-ce-ps3) (validado no RPCS3 e
em PS3 com HEN). O trio ps3aqua é corrigido entre si; não misturar com o ps3dev upstream.

| Componente | Commit | Origem |
| --- | --- | --- |
| Imagem base | `ubuntu:24.04` | Docker Hub oficial |
| ps3toolchain (GCC 7.5.0, binutils 2.42, newlib PS3) | `961fddac01337f18da08f4471d558a7a5b0d9af2` | github.com/ps3aqua/ps3toolchain |
| PSL1GHT | `af9d3d964c8faa69abce4961a269f9582e05a33f` | fixado por `scripts/008-psl1ght.sh` |
| SDL2_PSL1GHT (SDL 2.30.2) | `e4f14cb406ccbab7bc94f85cfd8c596fe4b7c419` | `SDL2_PSL1GHT/` vendorizado no fallout1-ce-ps3 |
| SELF/SFO/PKG | `make_self_npdrm`, `sfo.py`, `pkg.py`, `package_finalize` | instalados pelo PSL1GHT |

`docker/toolchain.lock.json` registra os mesmos commits, caminhos e flags; `setup` passa os
commits do lock como `--build-arg`, então lock e imagem não divergem. A primeira build leva
~30–60 min; depois fica em cache. Para não perder a imagem: `docker save pc2ps3/ps3dev:1 -o ps3dev.tar`.

Correção local: `autogen.sh` do SDL2_PSL1GHT está no commit sem bit de execução (exit 126);
o Dockerfile o executa com `sh`.

## Comandos

Na raiz, Windows:

```powershell
Copy-Item docker/toolchain.lock.example.json docker/toolchain.lock.json
# Preencher os campos com referências comprovadas antes do setup.
.\scripts\setup.ps1
.\scripts\build-ps3.ps1
.\scripts\package-ps3.ps1
.\scripts\test.ps1
.\scripts\clean.ps1
```

Linux:

```bash
cp docker/toolchain.lock.example.json docker/toolchain.lock.json
# Preencher os campos com referências comprovadas antes do setup.
bash scripts/setup.sh
bash scripts/build-ps3.sh
bash scripts/package-ps3.sh
bash scripts/test.sh
bash scripts/clean.sh
```

Os wrappers chamam `tools/phase0.py` e preservam o código de saída. Sucesso
retorna 0; configuração ausente, ferramenta ausente, build ou inspeção inválida
retornam 1 com erro. `--help` descreve o frontend. Não altere a política de
execução global do Windows para executar os wrappers; Python direto é equivalente:
`python tools/phase0.py setup`.

| Comando | Efeito/saída |
| --- | --- |
| setup | Valida o lock, constrói `pc2ps3/ps3dev:1`, registra ID e versões em `build/phase0/environment.json`, compila e linka um probe SDL2 PPU e inspeciona o ELF |
| build-ps3 | Compila `packaging/ps3/pc2ps3.elf` (runtime + backend SDL2 + `apps/ps3/main.cpp`); imprime header readelf e SHA-256 |
| package-ps3 | Gera `dist/ps3/pc2ps3.pkg` (RPCS3) e `pc2ps3.gnpdrm.pkg` (PS3 com HEN) e confere magic PKG, Content ID, EBOOT SELF, PARAM.SFO e os 4 membros |
| test | Builda e roda a suíte CTest do host (no contêiner se não houver CMake local) e depois `package-ps3` |
| clean | Prévia padrão, sem apagar; confirmação explícita remove apenas saídas geradas validadas |

`clean` limita-se a `build/`, `build-manual/`, `dist/`, `asset-manifest.json` e
`prototype.ppm`. Antes de qualquer remoção, confere todos os alvos existentes e
seus descendentes: caminhos resolvidos devem estar dentro da raiz; symlinks,
junctions e outros reparse points são rejeitados, assim como tipos inesperados.
Arquivos de fonte e `assets/` ficam fora da lista de remoção.

A pessoa pode executar a remoção após aprovação, conforme a governança do projeto:

```powershell
.\scripts\clean.ps1 -Confirm
```

```bash
bash scripts/clean.sh --confirm
```

Equivalente direto: `python tools/phase0.py clean --confirm`. A opção só é aceita
para `clean`; sem ela, nada é apagado. Não execute a remoção enquanto outro
processo altera os alvos: a validação prévia não oferece bloqueio contra mudanças
concorrentes no sistema de arquivos. A opção destrutiva não foi executada por Cloud.

## Pacote gerado

| Campo | Valor |
| --- | --- |
| TITLE / TITLE_ID | PC2PS3 Runtime Demo / `PCPS00001` (ID próprio, sem prefixo oficial BLUS/BLES/NPUB) |
| CONTENT_ID | `UP0001-PCPS00001_00-0000000000000000` |
| CATEGORY / BOOTABLE | HG / 1 |
| Membros | `USRDIR/EBOOT.BIN` (SELF NPDRM), `PARAM.SFO`, `ICON0.PNG` (320×176), `PIC1.PNG` (1920×1080) |

Desde a Fase 3 o executável é o vertical slice descrito em [PHASE3.md](PHASE3.md#vertical-slice-no-ps3-4-nov-2026). A primeira versão (Fase 1), validada abaixo, usava só conteúdo original: inicializa vídeo em 1280×720 (RSX
via SDL2_PSL1GHT, framebuffer de 640×360 escalado), lê o DualShock 3 (✕ pula, Start pausa,
Start+Select sai), toca música e efeitos gerados (AudioManager → SDL → cellAudio), desenha
sprite escalado e rotacionado, mostra FPS e heap no HUD e volta ao XMB pelo "Sair do jogo".
Registra cada etapa no TTY (`[pc2ps3] ...`).

## Teste no RPCS3

1. Instale o firmware oficial (PS3UPDAT.PUP, de playstation.com) no RPCS3: `File → Install Firmware`.
2. `File → Install Packages/Raps/Edats` → `dist/ps3/pc2ps3.pkg`.
3. Inicie "PC2PS3 Runtime Demo". No `TTY.log` devem aparecer `[pc2ps3] boot`, `video 640x360 -> 1280x720`,
   `audio 48000 Hz` e `demo started`; ao sair, `quit requested` e `exit status 0`.

PS3 físico com HEN: copie `pc2ps3.gnpdrm.pkg` para a raiz de um pendrive e instale pelo
Package Installer. O firmware e as configurações do console não são alterados.

## Evidência (2026-11-04)

- `docker build` da imagem `pc2ps3/ps3dev:1`: sucesso; `setup` imprimiu GCC 7.5.0, SDL 2.30.2 e o
  header do probe (`ELF64`, big endian, `PowerPC64`, `EXEC`).
- `package-ps3`: código 0; `pc2ps3.pkg` e `pc2ps3.gnpdrm.pkg` com ~1,05 MB cada (o tamanho exato varia por build; `package-ps3` imprime SHA-256 e bytes), magic `\x7fPKG`,
  Content ID no header, EBOOT com magic `SCE\0`, PARAM.SFO `\0PSF` com os campos acima.
- `test`: 67/67 testes CTest passaram, seguidos do empacotamento.
- **RPCS3 0.0.43-20561 (Vulkan, Async Shader Recompiler) + firmware oficial 4.91**: o RPCS3 leu o PKG
  (magic, Content ID `UP0001-PCPS00001...`, 5 entradas, NPDRM free). Boot do EBOOT em `dev_hdd0/game/PCPS00001`:
  janela "FPS: 60.0x | PC2PS3 Runtime Demo [PCPS00001]"; TTY:
  `boot` → `video 640x360 -> 1280x720` → `audio 48000 Hz` (porta cellAudio aberta, 2 canais, 8 blocos) →
  `demo started heap=2088960` → `pad connected` → `pad buttons=0x0200` (✕) / `axis=3,0` (analógico) →
  `pad buttons=0x0090` (Start+Select) → `start+select quit` → `demo stopped after 1227 ticks` → `exit status 0`;
  o processo do RPCS3 terminou com código 0. Captura nativa: [img/ps3-rpcs3-demo.png](img/ps3-rpcs3-demo.png)
  (jogador deslocado e no ar após analógico + ✕).
- Defeitos achados e corrigidos nesse teste: `mallinfo()` do newlib PPU devolve campos de 32 bits sob
  header `size_t` (HUD mostrava lixo) → heap medido por `sbrk`; o PSL1GHT não emite hot-plug do pad → o backend
  reabre o joystick a cada frame até conectar; glifo `+` ausente na fonte 3×5.
- **Ainda sem comprovação:** áudio audível (só a abertura da porta e o callback estão comprovados por log),
  D-pad (o perfil de teclado padrão do RPCS3 não o mapeia) e o console físico com HEN (`pc2ps3.gnpdrm.pkg`).
