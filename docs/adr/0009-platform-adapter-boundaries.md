# ADR 0009 — Fronteira dos adaptadores de plataforma
Data: 2026-11-02 · Status: aceita

## Contexto
O núcleo portátil já fornece contratos para input lógico, mixagem PCM16 e framebuffer. A Fase 1 precisa ligar esses contratos a dispositivos sem introduzir SDL2, PSL1GHT ou APIs de hardware no core. O host ainda não tem toolchain PS3 verificada.

## Decisão
1. Adaptadores ficam em `src/platform/` e suas interfaces públicas em `include/pc2ps3/`; eles dependem do core, e o core não depende dos adaptadores.
2. O adaptador de input recebe amostras já normalizadas pelo backend e preenche somente os campos DualShock 3 de `InputSnapshot`.
3. O callback de áudio delega ao `AudioManager`, limita cada chamada por um máximo de frames e zera a saída quando uma chamada falha ou lança exceção.
4. O presenter copia o `Framebuffer` para memória caller-owned com stride explícito e ordem RGBA/BGRA. Ele não abre janela nem controla RSX.
5. Nenhum desses contratos declara um backend SDL2/PSL1GHT pronto; esses drivers ficam em tarefas separadas quando as dependências forem verificáveis.

## Alternativas consideradas
- Incluir SDL2/PSL1GHT diretamente no core: rejeitado porque acopla a simulação a toolchains e APIs de dispositivo.
- Fazer os adaptadores armazenarem ou possuírem buffers/dispositivos: rejeitado porque muda o ownership e dificulta callbacks determinísticos.
- Expor o `std::vector<Color>` interno do framebuffer: rejeitado; a interface de pixels permanece como ponteiro const e dimensões.

## Consequências
Os contratos podem ser exercitados no host e alimentados por implementações reais depois. A cópia de pixels tem custo proporcional à área do framebuffer; o callback permanece single-threaded conforme o contrato do `AudioManager`. A integração real, latência, vídeo e boot PS3 seguem sem comprovação até existir toolchain e hardware/emulador compatíveis.

## Rollback
Remover os três adaptadores, seus testes/alvos CMake e este ADR. Os módulos existentes de `InputSnapshot`, `AudioManager` e `Framebuffer` permanecem utilizáveis.
