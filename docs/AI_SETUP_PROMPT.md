# Prompt de setup para agentes de IA

Copie o bloco abaixo e cole no seu agente de código (Claude Code, Codex, Cursor etc.) com o
repositório clonado e aberto como pasta de trabalho.

```text
Você vai preparar o ambiente de desenvolvimento do projeto PC2PS3 (framework C++11 para ports
nativos de jogos 2D no PlayStation 3, com toolchain PSL1GHT em Docker). Trabalhe na raiz do
repositório clonado. Antes de cada passo, diga o que vai fazer; se algo falhar, mostre o erro e
pare para perguntar em vez de improvisar.

Regras que não podem ser quebradas:
- Não baixe, copie nem gere conteúdo comercial (jogos, assets, executáveis, SDK oficial da Sony,
  chaves ou firmware). O repositório só contém código e arte autoral.
- Pastas como original-pc/, original-console/, games/cuphead/ e analysis/evidence/ são locais e
  ficam fora do Git. Não as crie com conteúdo de terceiros e não as versione.
- Não altere os commits fixados em docker/toolchain.lock.json.

1. Detecte o sistema operacional e confira as dependências, instalando o que faltar
   (pergunte antes de instalar):
   - Git.
   - Python 3.8+ (`python` no Windows, `python3` no Linux/macOS). As ferramentas em tools/ usam
     só a biblioteca padrão.
   - Docker (no Windows: Docker Desktop com engine Linux/WSL2, e confirme `docker info`).
   - Opcional para build/teste nativo do host: CMake 3.16+, Ninja e um compilador C++11
     (g++/clang ou MSVC). Sem eles, os testes rodam dentro do contêiner.
   - Opcional para rodar o PKG: o emulador RPCS3 (https://rpcs3.net), baixado pelo próprio usuário.

2. Build e testes do host:
   - Com CMake local:
       cmake -S . -B build -G Ninja
       cmake --build build
       ctest --test-dir build --output-on-failure
       ./build/pc2ps3_demo        (deve imprimir JSON com tick:180)
   - Sem CMake local: `bash scripts/test.sh` (Linux) ou `scripts/test.ps1` (Windows).
   - Testes Python: `python -m unittest discover -s tests -p "test_*.py"`.

3. Toolchain PS3 (primeira vez leva 30–60 min; usa rede só neste passo):
   - Linux:   bash scripts/setup.sh
   - Windows: powershell -ExecutionPolicy Bypass -File scripts/setup.ps1
   Isso constrói a imagem Docker pc2ps3/ps3dev:1 a partir de docker/ps3dev.Dockerfile.

4. Build e pacote PS3:
   - Linux:   bash scripts/build-ps3.sh && bash scripts/package-ps3.sh
   - Windows: scripts/build-ps3.ps1 e scripts/package-ps3.ps1
   O resultado fica em dist/ps3/ (pc2ps3.pkg para RPCS3, pc2ps3.gnpdrm.pkg para PS3 com HEN).
   Para um PKG público sem módulos locais, use o alvo com NO_LOCAL_GAME=1
   (veja packaging/ps3/Makefile).

5. Leia, nesta ordem, para entender o estado e o próximo marco: README.md, docs/STATUS.md,
   docs/ENTREGA.md, docs/PHASE0.md e docs/adr/.

6. Ao final, relate: o que foi instalado, versões (python, docker, cmake, compilador), resultado
   do ctest (passou/falhou com contagem), se o PKG foi gerado e qualquer passo pulado.
```
