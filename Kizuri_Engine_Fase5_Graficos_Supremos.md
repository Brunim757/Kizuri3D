# Fase 5 — Gráficos Supremos (só o que falta)

Continuação da Fase 5. **Não inclui** SSAO, Névoa volumétrica, Decals e Reflexos (SSR) — esses já foram implementados (eram o conteúdo do `Kizuri_Engine_Fase5_Graficos_Adicionais.md` anterior, que pode ser descartado). Daqui pra baixo é só o que ainda não foi feito.

Tudo abaixo é viável em **DX11**, sem depender de ray tracing por hardware (DXR é DX12-only, fora de escopo — ver TDD). A referência de linguagem visual é Dark Souls 3/Bloodborne/Sekiro, que também não usavam DXR.

Ordem sugerida por custo e dependência do que já existe:

## 5g — Hemisfério + Wrap Lighting
- Luz de ambiente/céu preenchendo sombras (cor do céu, cor do chão, intensidade)
- Wrap lighting (half-lambert) pra suavizar o terminator (transição luz/sombra)
- Resolve a maior parte do "preto chapado" pelo menor esforço de todos os itens restantes

## 5h — GI baked (lightmaps + light probes + reflection probes)
- Ferramenta de "bake" no Editor: processa luz indireta uma vez, offline, por cena
- Lightmaps pra geometria estática
- Light probes espalhados pelo espaço, pra objetos dinâmicos (personagens) amostrarem em runtime
- Reflection probes (cubemap com correção de paralaxe) pra reflexo especular de ambiente
- Mesma técnica usada nos jogos de referência do nicho

## 5i — SSGI (iluminação indireta em tela)
- Um bounce de luz indireta aproximado, calculado em cima do que já está visível na tela
- Complementa o GI baked pra objetos/situações que o bake não cobre

## 5j — Voxel Cone Tracing (GI dinâmica em tempo real)
- GI que reage a mudanças de iluminação sem precisar rebake
- Representação voxelizada da cena, compute shader, sem depender de DXR
- O mais caro e ambicioso desta lista — só depois de todo o resto estável

## 5k — Material e superfície avançados
- Parallax Occlusion Mapping (detalhe de profundidade em pedra/tijolo sem geometria extra)
- Subsurface scattering (pele de personagem, importante pra close-up de combate)
- Filtragem anisotrópica (qualidade de textura em ângulo raso)
- Contact shadows (micro-sombra que a resolução da shadow map normal perde)

## 5l — Pós-processamento cinematográfico
- Depth of Field (desfoque de profundidade, útil pra cutscene)
- Motion blur de câmera (reprojeção via view-projection atual vs. anterior)
- Vinheta, grão de filme, aberração cromática — acabamento de atmosfera sombria

## 5n — LOD e detalhe de sombra
- Sistema de LOD com transição suave (crossfade, sem "pop" visível)
- Complementa o instancing/culling já obrigatório desde a 5a
