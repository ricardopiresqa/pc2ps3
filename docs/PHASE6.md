# Fase 6 — primitivas genéricas de combate

Esta etapa prepara o core para adapters de jogos de ação sem codificar regras,
assets, números ou sequências de Cuphead. Ela reutiliza `HitboxSystem`,
`ProjectileSystem` e `BossEncounter` existentes.

## Módulos

- `CombatResolver` converte os `HitEvent` de um tick e perfis de ataque do
  chamador em `DamageEvent`. Dano é apenas um dado de saída: o adapter decide se
  e como altera vida, invulnerabilidade ou pontuação. Resultados repetidos para
  o mesmo ataque e alvo são deduplicados na chamada.
- `ParryWindow` mede uma janela em ticks de simulação. O adapter abre a janela
  ao receber sua ação de parry; o módulo só aceita contatos declarados como
  parryable e não escolhe input, recompensa ou efeito. O tick de ativação conta
  como o primeiro dos ticks configurados.
- `ProjectilePattern` percorre steps emprestados de `ProjectileSpawn`, emitindo
  cada volley no tick correspondente. O chamador encaminha as saídas a
  `ProjectileSystem`; os offsets devem ser não decrescentes, o scheduler não
  move, colide ou aplica dano, e a configuração precisa sobreviver ao objeto.
- `WeaponSystem` seleciona perfis caller-provided e emite um `WeaponFireEvent`
  quando a arma está pronta. Cooldown é por arma; dano, munição e criação de
  projéteis continuam no adapter.
- `BossPatternTimeline` seleciona uma lista de action IDs pelo phase ID e emite
  ações nos offsets configurados. Ela não escolhe padrões, IA ou efeitos; o
  adapter pode encaminhar IDs ao `ProjectilePattern` e a outros sistemas.
- `CoopInputRouter` guarda dois `InputFrame`s já mapeados por tick sem misturar
  jogadores. Amostragem de hardware e associação de controles ficam nos
  adapters de plataforma.

As configurações são limitadas, nenhuma atualização aloca memória e arrays
emprestados devem permanecer vivos enquanto o componente os utilizar.

## Verificação

Os alvos CTest incluem `animation_playback`, `animation_events`,
`combat_resolution`, `parry_window`, `projectile_pattern`, `weapon_system`,
`boss_pattern`, `coop_input_router`, `local_coop_session` e
`phase6_combat_integration`.
Uma execução manual de teste não substitui CTest nem prova compatibilidade C++11.
Não há afirmação de execução em RPCS3 ou hardware PS3 nesta fase.
## Fase 6C — eventos, co-op e integração sintética

`AnimationFrame` aceita um `event_id` opcional (zero significa ausência de
marcador). `AnimationPlayer::update_with_events` emite o marcador inicial e os
marcadores das frames atravessadas em ordem, inclusive em loops. A operação
pré-valida capacidade e limite de 4096 transições; falhas não alteram o player
nem o buffer de eventos. Para clips com marcadores, use essa API em todos os
ticks; `update` continua preservando o comportamento legado e não reporta
eventos.

`LocalCoopSession` mantém referências não proprietárias a dois `World`s
distintos e a um `CoopInputRouter`. O runner exige os dois frames antes de
avançar qualquer mundo, sem tratar input físico, geometria compartilhada ou
colisão entre jogadores.

`apps/headless/phase6_combat_demo.cpp` encadeia arma, spawn authored,
hitbox/contato, resolução de dano, parry e timeline de fases do chefe. Em
sucesso publica um único resumo JSON determinístico; em falha retorna código
não zero antes de emitir JSON parcial. O CTest executa a demo, enquanto
`tests/phase6_combat_integration_test.cpp` conserva as asserções de contrato.
IDs, valores, caixas e cores são sintéticos; não são dados de jogo específico.
