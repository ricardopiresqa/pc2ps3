# ADR 0015: primitivas genéricas de campanha

- Status: aceito para implementação local; C++ ainda não validado por build/CTest.
- Data: 2026-11-02.

## Decisão

Adicionar três componentes portáteis e independentes:

- `MenuFlow` modela menu principal, pausa, configurações e confirmação de saída.
  Ele emite comandos; o Game Adapter desenha e executa os efeitos.
- `CampaignProgression` mantém até 64 fases e persiste um payload v1 little-endian
  através do `SaveManager` existente.
- `BossEncounter` controla vida e troca de fase por limiares fornecidos pelo
  Game Adapter. Ataques, animações e conteúdo permanecem fora do core.

## Motivo e alternativas

O prompt pede expansão de menus, saves/progressão e chefes, mas ainda não há
especificação validada nem direitos confirmados para conteúdo específico de
Unity. Componentes genéricos atendem ao framework sem embutir assets comerciais
ou padrões de jogo não verificados.

Implementar telas, chefes e assets específicos agora foi descartado por falta
de especificação/licença e backend validado. Criar armazenamento paralelo foi
descartado porque `SaveManager` já fornece slots, limite e escrita atômica.

## Consequências

- Integração visual e efeitos de plataforma continuam sob o Game Adapter.
- O formato v1 só aceita máscaras sequenciais compatíveis com a progressão do
  runtime; payload incompatível/corrompido não altera estado ativo.
- Nenhum componente comprova fidelidade a Unity sem dados, licença e QA do jogo.
- Compilação C++11 e CTest continuam gates obrigatórios antes de fechar os cards.

## Rollback

Remover os três módulos, testes e alvos CMake da Fase 5, além desta ADR e das
entradas de documentação correspondentes. Não exige migração de saves existentes:
nenhum dado v1 foi distribuído.

## Recibo Thoth

- Digest: `be4b489b2a0e326a3dfc87aa0d166192617b8c15e6e7b93f7f709914bb70ffeb`
- Signature: `076a6287b72166fb69663ff7632cc21a886d52b1fdb0c71506012985f0376472`
