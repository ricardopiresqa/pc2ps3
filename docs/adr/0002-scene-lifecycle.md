# ADR 0002 — Lifecycle e transições determinísticas de cena
- Status: decisão de arquitetura para implementação; validação pendente.
- Escopo: TaskSpec `pc2ps3-phase2-scenes`.

## Contexto
O runtime C++11 precisa trocar cenas sem destruir uma cena durante o próprio callback e sem acoplar o núcleo a renderer, arquivos ou APIs de plataforma.

## Decisões
1. `SceneManager` possui uma cena ativa e, no máximo, um destino pendente via `std::unique_ptr`.
2. `request_transition()` também seleciona a primeira cena. O pedido é aplicado no próximo `update(dt)` válido.
3. Um pedido feito fora de `update()` é aplicado antes da atualização seguinte. Um pedido feito pelo callback `update()` atual só é aplicado depois que ele retornar. Cada chamada pode aplicar no máximo duas transições.
4. A troca chama `on_exit()` da cena antiga, destrói a cena antiga, instala a nova e chama `on_enter()` da nova.
5. Pedidos nulos ou duplicados, pedidos durante callbacks de lifecycle/destruição e chamadas reentrantes de `update()` são rejeitados. A operação recebe ownership por valor; um destino rejeitado é consumido e destruído.
6. Callbacks e destrutores não lançam exceções. O manager é single-threaded, não copiável nem movível. O ponteiro da cena ativa é observador e só permanece válido até a próxima transição.
7. Deltas negativos ou não finitos são ignorados sem mudar o estado ou consumir uma transição pendente; delta zero é válido.

## Alternativas consideradas
- Trocar imediatamente: pode destruir uma cena enquanto seu callback ainda está executando.
- Manter uma fila ilimitada: permite encadeamento não limitado e aumenta estado e uso de memória sem requisito atual.
- Usar uma API separada para inicializar a cena: duplica o caminho de entrada e saída.

## Consequências
As transições têm ordem observável e determinística. Cenas não podem iniciar transições em callbacks `on_enter()` ou `on_exit()`, e destinos rejeitados são destruídos no retorno de `request_transition()`.
O módulo continua independente de plataforma e pode ser removido sem alterar os demais módulos.

## Evidências necessárias
- Teste permanente verificando callbacks, ordem e momento de destruição, requests inválidos/duplicados, reentrância e preservação de pendência com `dt` inválido.
- Execução via CTest e QA independente; ausência de toolchain deve permanecer explicitamente bloqueada.

## Rollback
Remover `SceneManager`, seu alvo CMake e os testes/documentação desta TaskSpec. Preservar os módulos existentes e não usar limpeza ou reset destrutivo.
