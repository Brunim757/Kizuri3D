# Kizuri Physics — Lista de Implementação (Fase 6)

## 1. Integração Jolt

- [ ] FetchContent Jolt (v5.0.0) no ThirdParty.cmake
- [ ] Biblioteca KizuriPhysics (Engine/Physics)
- [ ] Trace/Assert handlers do Jolt
- [ ] Broadphase layers (estático/dinâmico) + filtros
- [ ] TempAllocator (10MB)
- [ ] Adaptador JobSystem Jolt → enkiTS (scheduler único, sem threads extras)
- [ ] Factory + registro de tipos

## 2. Corpos Rígidos

- [ ] Criação/destruição de corpos (static/dynamic/kinematic)
- [ ] Massa customizada + inércia calculada
- [ ] Atrito e restituição por corpo
- [ ] Teleporte de estáticos/cinemáticos
- [ ] Alvos de movimento cinemático (MoveKinematic por substep)
- [ ] Mapa entidade → corpo com handles + geração

## 3. Colisores

- [ ] Box
- [ ] Sphere
- [ ] Capsule
- [ ] Mesh (triângulos, estático)

## 4. Simulação

- [ ] Passo fixo 60Hz com acumulador (máx 4 substeps)
- [ ] Interpolação posição/rotação passada ao render (anti-tremor)
- [ ] Gravidade configurável
- [ ] Velocidade de plataforma aplicada antes da colisão (MoveKinematic)

## 5. Queries

- [ ] Raycast (posição, normal, corpo atingido)
- [ ] Teste: queda livre (gravidade reduz Y)
- [ ] Teste: caixa repousa no chão (altura + repouso)
- [ ] Teste: raycast acerta o chão

## 6. Ragdoll / Joints

- [ ] API de constraints (point, hinge)
- [ ] Montagem de ragdoll a partir de caixas
- [ ] Teste: ragdoll cai sem explodir (posições finitas)

## 7. Editor

- [ ] Componente Rigidbody no ECS (segue padrão hasX)
- [ ] Undo/Redo (Add/Set/Remove via EditQueue)
- [ ] Inspector de Rigidbody (tipo, massa, colisor, tamanhos, atrito)
- [ ] Serialização RIGIDBODY na cena
- [ ] Wireframe de colisores no Viewport (box/sphere/capsule)
- [ ] Toggle Simulate no viewport + passo por frame
- [ ] Poses interpoladas usadas no render sem sujar a cena

## 8. Fora do escopo (v1)

- [ ] VehicleController (fora do nicho souls-like atual)
- [ ] Soft bodies
- [ ] Destruição com fratura (Jolt permite, decisão do jogo)
