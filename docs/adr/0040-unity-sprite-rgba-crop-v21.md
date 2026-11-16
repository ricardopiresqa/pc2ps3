# ADR 0040 - recorte Sprite RGBA bounded

- Status: accepted
- Data: 2026-11-03
- TaskSpec: `pc2ps3-phase3-sprite-rgba-crop-v21`

## Contexto

O decoder Texture2D retorna RGBA8 e o leitor Sprite retorna `m_Rect` em coordenadas Unity com origem inferior esquerda. A ordem de linhas do buffer pode variar entre origem/conteudo; nao se deve inferir a orientacao visual do payload.

## Decisao

Adicionar `crop_sprite_rgba(image, region, source_origin=...)`. A origem do buffer e obrigatoria (`top-left` ou `bottom-left`); o resultado e normalizado para ordem PNG top-left. Dimensoes, buffer exato, limites, tipos e bounds sao validados antes de alocar; canais RGBA permanecem intactos. A funcao so opera em memoria e nao grava nem imprime pixels.

## Validacao

Fixtures de 3x2 com cores e alpha diferentes confirmam recorte exato para as duas origens. Testes focados: 19 passed e 25 subtests. Regressao PC2PS3: 264 passed, 7 skipped e 207 subtests. Argus e Tifa aprovaram; cinco criterios Thoth estao PASS. A orientacao do payload real do build Unity analisado continua requerendo escolha explicita do chamador.
