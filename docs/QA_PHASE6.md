# QA — Fase 6

## Combat resolution

```gherkin
Feature: converter contatos em eventos de dano
  Scenario: contatos repetidos em hurtboxes do mesmo alvo
    Given dois contatos do mesmo ataque contra hurtboxes do mesmo alvo
    When os contatos são resolvidos com um perfil válido
    Then somente um DamageEvent é emitido para esse ataque e alvo
  Scenario: perfil desconhecido ou saída pequena
    Given um contato sem perfil correspondente ou capacidade de saída insuficiente
    When a resolução é solicitada
    Then a operação falha e não grava saída parcial
```

## Parry window

```gherkin
Feature: controlar uma janela de parry em ticks
  Scenario: duração exata da janela
    Given uma janela configurada para três ticks
    When o chamador a aciona e avança três ticks
    Then ela aceita contato parryable em cada tick e expira no seguinte
  Scenario: contato sem propriedade parryable
    Given a janela ativa
    When chega um contato não parryable
    Then o contato não é aceito
```

## Projectile pattern

```gherkin
Feature: emitir volleys caller-authored
  Scenario: dois spawns no mesmo tick e outro em tick posterior
    Given steps ordenados nos ticks zero e dois
    When o scheduler avança os ticks
    Then cada spawn é emitido uma vez e na ordem definida
  Scenario: capacidade insuficiente
    Given mais spawns no tick atual do que cabem na saída
    When o scheduler avança
    Then falha sem avançar o cursor ou alterar a saída
```

## Limite do ambiente

Os cenários devem ser compilados e executados no alvo de cada módulo e no
CTest. Se a toolchain só permitir MSVC `/std:c++14`, registrar essa execução
separadamente e manter C++11/CTest como `NOT_PROVEN`.

## Weapon system

```gherkin
Feature: selecionar arma e emitir disparos sem regras de jogo
  Scenario: cooldown é independente por arma
    Given duas armas com IDs e cooldowns caller-provided
    When o jogador troca de arma enquanto a primeira está em cooldown
    Then cada arma preserva seu próprio contador e só emite ao ficar pronta
  Scenario: perfil inválido ou arma inexistente
    Given perfis duplicados ou uma seleção desconhecida
    When o sistema valida ou seleciona
    Then falha sem substituir a seleção válida
```

## Boss patterns

```gherkin
Feature: emitir ações por fase do chefe
  Scenario: fase contém várias ações e um tick vazio
    Given uma sequência ordenada para uma fase válida
    When o timeline avança pelos offsets
    Then emite os IDs caller-authored na ordem e não emite no tick vazio
  Scenario: fase desconhecida ou saída insuficiente
    Given uma fase ativa e capacidade insuficiente
    When inicia uma fase desconhecida ou avança a saída cheia
    Then preserva a fase, cursor e buffer atuais
```

## Local co-op input

```gherkin
Feature: rotear frames locais de dois jogadores
  Scenario: frames independentes no mesmo tick
    Given frames já mapeados diferentes para dois slots
    When ambos são submetidos
    Then cada slot lê apenas o próprio frame
  Scenario: slot duplicado e próximo tick
    Given o slot zero já recebeu um frame
    When uma segunda submissão é tentada e começa novo tick
    Then a duplicata não sobrescreve e begin_tick limpa os dois slots
```
## Fase 6C — cenários e evidências

```gherkin
Feature: Marcadores de animação
  Scenario: Emitir markers iniciais, atravessados e em loops
    Given frames com markers 11, 22 e 0 e durações 0.1, 0.2 e 0.1
    When update_with_events avança 0.95 segundos
    Then emite 11@0, 22@1, 11@0, 22@1, 11@0 e 22@1 nessa ordem
    And o índice e o tempo final coincidem com AnimationPlayer::update

  Scenario: Falhar atomicamente quando o buffer é curto ou o delta é inválido
    Given um clip com marker inicial e marker na próxima frame
    When a capacidade é 1 para dois eventos, ou o delta é NaN
    Then a chamada falha com count zero, buffer intacto e player intacto

  Scenario: Reset e limite de transições
    Given um clip com marker inicial e um loop sem markers
    When reset é seguido de delta zero, ou um delta excede 4096 transições
    Then reset emite o marker inicial e excesso falha sem mutação
```

```gherkin
Feature: Sessão co-op local
  Scenario: Isolar frames e avançar dois mundos em ticks consecutivos
    Given dois Worlds distintos e move_x oposto por slot
    When ambos os frames são submetidos em dois ticks
    Then cada World avança uma vez com o próprio frame

  Scenario: Não avançar parcialmente
    Given apenas o frame do slot zero
    When LocalCoopSession::step é chamado
    Then retorna false e as posições dos dois Worlds permanecem iguais
    And uma sessão que referencia o mesmo World duas vezes é inválida
```

```gherkin
Feature: Integração headless de combate
  Scenario: Levar um disparo sintético até a troca de fase
    Given arma, spawn, contato, perfil de dano e limiar sintéticos
    When o disparo gera contato e o contato vira DamageEvent
    Then BossEncounter reduz HP e avança para fase um
    And BossPatternTimeline emite ação da fase zero e depois da fase um

  Scenario: Respeitar a janela de parry
    Given uma janela de um tick
    When o tick é aberto e depois expira
    Then contato parryable é aceito durante a janela e rejeitado após ela
```

Execução em 2026-11-02: MSVC / CMake, configuração Debug. Os executáveis
`animation_events`, `local_coop_session` e `phase6_combat_integration` passaram
diretamente e via CTest. Compatibilidade com C++11 estrito permanece
`NOT_PROVEN`: o toolchain disponível traduz o padrão mínimo para MSVC C++14.

## Execução da Fase 6B — WeaponSystem, BossPatternTimeline e CoopInputRouter

Tifa executou os cenários Gherkin desta seção em 2026-11-02. O build Debug do
CMake/Ninja com MSVC 19.44 compilou os alvos dos três módulos e os demais alvos
C++ do projeto; CTest passou em 58/58 testes. Os três testes focados passaram
em 3/3. A configuração usou `/Z7` para contornar `C1902` do PDB; isso não
alterou fontes nem opções do projeto.

Resultado dos cenários: troca de arma preserva cooldowns independentes,
seleção inválida preserva a seleção, cooldown zero permite disparo por tick e
reset limpa o estado; o timeline emite ações simultâneas e futuras em ordem,
respeita ticks vazios e capacidade atômica, preserva a fase ativa em seleção
inválida e reseta; o roteador mantém frames isolados, rejeita duplicatas e
índices inválidos, limpa os slots no novo tick e deixa slot ausente indisponível.

`CMakeLists.txt` solicita C++11, mas esta execução no MSVC 19.44 não prova modo
estrito C++11. O WSL disponível retornou `Wsl/E_ACCESSDENIED`; portanto, os
critérios que exigem compilador/CTest estritamente C++11 continuam
`NOT_PROVEN`. Nenhum teste executou em RPCS3 ou hardware PS3.
## Reexecucao da Fase 6C (2026-11-02)

Tifa executou os cenarios dos tres modulos. CTest focado passou em 3/3
(`animation_events`, `local_coop_session`, `phase6_combat_integration`); a
regressao C++ completa passou em 58/58. AnimationEvent e LocalCoopSession
passaram funcionalmente, mas os testes em modo estrito C++11 seguem
`NOT_PROVEN` pelo limite do MSVC/WSL descrito acima. A integracao headless
passou todos os cinco criterios com valores sinteticos. Sem execucao em PS3 ou
RPCS3.
## Reexecucao das primitivas de combate da Fase 6 (2026-11-02)

Tifa executou `combat_resolution`, `parry_window` e `projectile_pattern` pelo
CTest focado: 3/3 passaram. A regressao C++ completa permanece 58/58. Dedup,
ordem deterministica, falhas atomicas, duracao exata de parry, volleys, ticks
vazios e reset passaram. Os tres testes em modo estrito C++11 ficam
`NOT_PROVEN`: MSVC 19.44 nao oferece esse modo e WSL retornou
`Wsl/E_ACCESSDENIED`. Sem execucao em PS3/RPCS3.

## Revalidacao Fase 6C e demo headless (2026-11-03)

O checkout tinha testes inline para a integracao, mas faltava o artefato
`apps/headless/phase6_combat_demo.cpp` exigido pelo TaskSpec. A demo foi
adicionada e o CTest agora executa a demo junto com os testes de animação,
sessão co-op e contrato da integração.

Resultado: `animation_events`, `local_coop_session`,
`phase6_combat_integration` e `phase6_combat_demo` passaram (4/4). A demo foi
executada duas vezes: exit code 0 nas duas, JSON idêntico e valores esperados
para spawn, dano, HP/fase, ações pré/pós-transição e parry. LLVM 19
`clang-tidy` aceitou os arquivos C++ da mudança com `-std=c++11`; não houve
link/runtime C++11 estrito nem execução em PS3/RPCS3.

A regressão CTest completa do host passou em 69/69 testes. O build completo
precisou ser sequencial; a evidência geral está em `QA_HOST_FULL.md`.

## Revalidação dos limites de WeaponSystem e BossPatternTimeline

```gherkin
Feature: validar limites caller-authored da Fase 6
  Scenario: aceitar exatamente a capacidade de armas e cooldown máximo
    Given 16 perfis de arma com IDs únicos e cooldown de até 65535 ticks
    When WeaponSystem é construído e uma arma é disparada
    Then a configuração é válida e o cooldown máximo permanece bounded
  Scenario: rejeitar arma acima da capacidade e limpar todos os cooldowns
    Given 17 perfis válidos
    When WeaponSystem é construído
    Then a configuração é inválida
    When um sistema válido com cooldowns em duas armas é resetado
    Then a primeira arma é selecionada e ambos os cooldowns são zero
  Scenario: aceitar o limite de fases e ações
    Given 8 fases com IDs distintos e 128 ações ordenadas em cada fase
    When BossPatternTimeline é construído
    Then a configuração é válida
  Scenario: rejeitar limites excedidos e offsets inválidos
    Given 9 fases, 129 ações em uma fase ou offset maior que 65535
    When BossPatternTimeline é construído
    Then a configuração é inválida sem ativar uma sequência
```
