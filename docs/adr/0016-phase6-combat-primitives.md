# ADR 0016. Primitivas genéricas de combate, parry e projéteis

Data: 2026-11-02 · Status: aceita para implementação local

## Contexto

A Fase 6 do prompt pede suporte a combate, parry, projéteis, chefes e animações
mais complexas. O runtime já tem `HitboxSystem` para detectar contatos,
`ProjectileSystem` para simular projéteis individuais e `BossEncounter` para
fases por limiares de vida. Não há especificação validada para regras ou padrões
de Cuphead, nem licença confirmada para assets específicos.

## Decisão

- `CombatResolver` recebe contatos atuais e perfis de ataque fornecidos pelo
  adapter e devolve `DamageEvent` determinísticos, sem alterar HP ou aplicar
  regras de jogo.
- `ParryWindow` acompanha uma janela em ticks iniciada pelo chamador e informa
  se um contato declarado parryable pode ser aceito.
- `ProjectilePattern` percorre uma sequência caller-authored de `ProjectileSpawn`
  e emite os itens por tick; o chamador os encaminha ao `ProjectileSystem`.
- Os três módulos ficam no core, com limites explícitos e sem alocação no
  caminho por tick. Adapters continuam donos de input, HP, recompensas, efeitos,
  AI e conteúdo.

## Alternativas consideradas

- Fazer `HitboxSystem` alterar HP foi descartado; isso misturaria detecção e
  regra de jogo e contrariaria o ADR 0006.
- Codificar padrões, números, assets ou comportamento de jogos comerciais foi
  descartado por falta de especificação e licença confirmadas.
- Adicionar scripting ou dependência externa foi descartado; sequências
  limitadas e tipos existentes atendem ao escopo.

## Consequências

- `CombatResolver` depende apenas do contrato `HitEvent`; não depende de
  `EnemySystem` nem decide saúde ou invulnerabilidade.
- `ProjectilePattern` reutiliza `ProjectileSpawn`; seu array de configuração é
  emprestado e precisa sobreviver ao scheduler.
- Os adapters precisam fornecer dados e encaminhar os eventos emitidos.
- Testes executados fora de CTest ou em `/std:c++14` não comprovam o gate C++11/
  CTest; esse estado deve permanecer explícito até uma toolchain compatível.
