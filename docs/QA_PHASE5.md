# QA — Fase 5

## Menu flow

```gherkin
Feature: navegar menus sem efeitos de plataforma
  Scenario: continuar apenas quando há save
    Given o menu principal está aberto
    When continuar é selecionado sem save
    Then a página permanece no menu e nenhum comando é emitido
  Scenario: configurações retornam à página de origem
    Given configurações foram abertas a partir da pausa
    When o usuário cancela
    Then a pausa volta a ser a página atual
  Scenario: sair exige confirmação
    Given o menu principal está aberto
    When voltar e depois confirmar são acionados
    Then o controlador emite apenas um pedido de saída ao chamador
```

## Campaign progression

```gherkin
Feature: persistir progressão de campanha
  Scenario: completar fase desbloqueia a próxima
    Given a primeira fase está desbloqueada
    When ela é concluída
    Then a primeira fica concluída e a segunda fica desbloqueada
  Scenario: save corrompido não altera a campanha ativa
    Given uma progressão válida está carregada
    When um payload inválido é lido
    Then o load falha e o progresso em memória permanece igual
```

## Boss encounter

```gherkin
Feature: avançar fases por limiares de vida
  Scenario: um golpe cruza mais de um limiar
    Given limiares decrescentes válidos
    When um golpe deixa a vida abaixo de ambos
    Then a fase avança diretamente para a fase correspondente
  Scenario: dano letal satura vida
    Given um chefe ativo
    When o dano excede a vida restante
    Then a vida fica em zero e o encontro é derrotado sem underflow
```

## Execução

Alvos CTest: `menu_flow`, `campaign_progression` e `boss_encounter`.

## Execução local (2026-11-03)

Tifa executou os cenários Gherkin pelos três alvos CTest focados:

- MenuFlow: transições principal/pausa/configurações, comandos de continuar e novo jogo, confirmação/cancelamento de saída e reset.
- CampaignProgression: desbloqueio/selecção, reset, round-trip SaveManager e rejeição atômica de payload corrompido/limites.
- BossEncounter: cruzar múltiplos limiares, dano letal saturado, reset e limiares inválidos.

Resultado: `menu_flow`, `campaign_progression` e `boss_encounter` passaram (3/3) no build Debug MSVC 19.44. LLVM 19 `clang-tidy` aceitou sintaticamente os seis arquivos com `-std=c++11`, sem link/runtime nesse modo. O CTest executado em C++11 estrito e a validação PS3 seguem `NOT_PROVEN`.
