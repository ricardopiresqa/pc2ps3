# Entrega — PKG PS3 `PCPS00001` (4 nov 2026)

## O que é

`dist/ps3/pc2ps3.pkg` (RPCS3) e `dist/ps3/pc2ps3.gnpdrm.pkg` (PS3 com HEN) instalam o
**PC2PS3 Runtime Demo**. É um vertical slice nativo PPU (PSL1GHT + SDL2_PSL1GHT), feito sobre
o runtime portátil do repositório.

O slice tem:
- correr, pular, atacar;
- um checkpoint, um inimigo com patrulha e vida, morte com respawn;
- uma tela STAGE CLEAR com tempo e recorde;
- save persistente, música e efeitos gerados;
- saída limpa para o XMB.

O PKG não leva nenhum asset comercial. Se houver um atlas animado gerado localmente em
`USRDIR/player/`, o personagem usa esses frames. Sem ele, o jogo
usa a arte original.

**Este não é o port completo do Cuphead.**

## Gerar o PKG

Requisitos: Docker Desktop (contêineres Linux) e Python 3.

```powershell
scripts/setup.ps1        # 1ª vez: imagem pc2ps3/ps3dev:1 com a toolchain fixada (~30-60 min)
scripts/test.ps1         # 70 testes CTest no contêiner + build + verificação do PKG
scripts/package-ps3.ps1  # só o PKG: dist/ps3/pc2ps3.pkg e pc2ps3.gnpdrm.pkg
```

`package-ps3` confere:
- a magic `\x7fPKG` e o title id dentro do PKG;
- o layout `USRDIR/EBOOT.BIN` (SCE), `PARAM.SFO` (PSF), `ICON0.PNG` e `PIC1.PNG`;
- o SHA-256 de cada arquivo.

## Instalar e jogar

### RPCS3

Forma 1, pela interface: `File > Install Packages/Raw Data` e escolha `dist/ps3/pc2ps3.pkg`.
Essa forma abre um diálogo de confirmação. A forma testada nesta entrega é a 2.

Forma 2, sem interface, copiando o layout:

```powershell
Copy-Item -Recurse dist/ps3/layout/* <rpcs3>/dev_hdd0/game/PCPS00001/
<rpcs3>/rpcs3.exe --no-gui <rpcs3>/dev_hdd0/game/PCPS00001/USRDIR/EBOOT.BIN
```

O RPCS3 precisa do firmware 4.91 instalado (`--installfw`). Recomendação: Async Shader Recompiler.

### PS3 com HEN

Copie `pc2ps3.gnpdrm.pkg` para um pendrive FAT32 e instale pelo Package Manager do HEN.
Nada disso altera firmware nem configurações. **Ainda não foi testado em console físico.**

### Controles

| Botão | Ação |
|---|---|
| Analógico esquerdo ou D-pad | correr |
| ✕ | pular |
| □ | atacar |
| Start | pausar; na tela de resultado, jogar de novo |
| Start + Select | sair para o XMB |
| L1 / R1 | percorrer sprites, só com um atlas estático de um único sprite |

## Módulo local opcional (`games/`, fora do Git)

`packaging/ps3/Makefile` compila um módulo de jogo local quando ele existe em `games/` (pasta
ignorada pelo Git). Um PKG gerado assim é de **uso pessoal e não deve ser distribuído**.

Para o PKG público, use `make NO_LOCAL_GAME=1` em `packaging/ps3`, ou gere o pacote numa cópia
do repositório sem a pasta. O EBOOT só abre o módulo se os dados dele estiverem em `USRDIR`;
sem eles, roda o slice.

## QA automática no RPCS3

```powershell
python tools/qa_rpcs3.py <pasta do rpcs3>
```

O script:
1. copia o layout e cria `USRDIR/qa_autoplay`;
2. inicia o EBOOT. O jogo corre sozinho até a meta, salva e sai;
3. confere os marcadores do TTY (boot, audio, checkpoint, victory, `save write status=0`,
   `exit status 0`);
4. salva 3 framebuffers em `USRDIR/qa_frame_{60,130,1100}.ppm`.

Não precisa de foco de janela nem de injeção de teclas.

Resultado desta entrega, com RPCS3 0.0.43-20561 e FW 4.91:
- `victory ticks=588 enemy_defeated=1 deaths=2`, idêntico em todas as execuções (simulação
  determinística);
- `save write status=0`, `exit status 0`, `missing markers: none`;
- o caminho com atlas local e o caminho com a arte original passaram.

## Comprovado e não comprovado

| Critério do prompt | Situação |
|---|---|
| Boot | RPCS3: comprovado |
| Vídeo | RPCS3: comprovado (720p, framebuffer conferido) |
| Input | RPCS3: comprovado (analógico, ✕, □, Start, Select). D-pad não comprovado |
| Áudio | dispositivo aberto a 48 kHz e mixer ativo; **não verificado de forma audível** |
| Saída | RPCS3: comprovada (Start+Select e evento do XMB levam a `exit status 0`) |
| Save | RPCS3: comprovado entre boots |
| Console físico com HEN | **não testado** |
| Cuphead | nenhum conteúdo no repositório; só primitivas genéricas de combate (Fase 6) e o módulo local opcional acima |

## Referência

- PKG: `TITLE_ID PCPS00001`, `CONTENT_ID UP0001-PCPS00001_00-0000000000000000`, categoria HG.
  O ID é próprio e não usa nenhum prefixo oficial.
- Toolchain: [docker/toolchain.lock.json](../docker/toolchain.lock.json), com ps3toolchain,
  PSL1GHT e SDL2_PSL1GHT em commits fixos.
- Detalhes: [PHASE0.md](PHASE0.md) (toolchain e PKG) e [PHASE3.md](PHASE3.md) (slice e atlas).
