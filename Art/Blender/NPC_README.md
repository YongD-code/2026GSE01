# NPC 블렌더 에셋

NPC1.png / NPC2.png의 특징을 참고해 주인공과 같은 제작 구조로 만든 3D 모델입니다. 원본 손그림을 그대로 복제한 모델은 아닙니다.

## 모델 구성

- `npc1.blend`: 갈색 단발, 얼굴과 눈, 가죽 조끼와 어깨띠, 겹치마, 허리 주머니, 붉은 문양의 부적, 철제 망치, 부츠. 망치는 오른팔 뼈대에 연결하고 이동 중 팔의 흔들림을 줄였습니다.
- `npc2.blend`: 분홍색 짧은 머리, 얼굴과 눈, 붉은 목도리와 등으로 내려진 후드, 재킷 단추, 붉은 소매·부츠 장식, 등 문장.
- 공통: 관절 가중치가 있는 팔·다리, 좌우 발이 교대하는 걷기와 달리기, 작은 호흡 대기 동작, 편집 가능한 액션과 재질 노드.

## 다시 생성하기

저장소 루트에서 실행합니다. 기존 npc1/npc2 블렌더 파일과 같은 이름의 출력물을 덮어쓰므로 수동 편집한 모델은 다른 이름으로 저장하세요.

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --python Art/Blender/create_npcs.py -- --npc npc1
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --python Art/Blender/create_npcs.py -- --npc npc2
```

끝에 `--preview-only`를 붙이면 모델과 미리보기만 생성합니다. `create_npcs.py`는 `create_protagonist.py`의 뼈대·표면 생성 함수·애니메이션·출력 방식을 재사용합니다. NPC 생성으로 주인공의 blend/PNG는 덮어쓰지 않습니다.

## 출력과 게임 적용

출력 폴더: `SimpleGame/Assets/Characters/Blender/npc1`, `npc2`.

- `preview.png`: 768×768 모델 미리보기.
- `idle.png`: 192×192 셀, 8열×8행, 호흡 동작 8프레임.
- `walk.png`, `run.png`: 192×192 셀, 9열×8행, 첫 열 대기 및 8프레임 이동 주기.
- 행 순서: 아래 / 왼쪽 아래 / 왼쪽 / 왼쪽 위 / 위 / 오른쪽 위 / 오른쪽 / 오른쪽 아래.
- `fixedCanvas=true`와 기존 카메라 기준점을 사용합니다. SpriteLayout에서 대기는 firstWalkFrame=0, 이동은 firstWalkFrame=1로 설정합니다.
- 기존 미라·라엔 자리에 새 대기 시트를 적용했습니다. 주변 플레이어를 바라보는 동작과 퀘스트 배치를 유지합니다. 원본 NPC PNG도 보존하며 새 시트가 없으면 원본을 사용합니다.
- 걷기·달리기 시트는 이동하는 NPC에 사용할 수 있도록 제작했습니다. 이번 작업에서 자율 이동 AI나 NPC 달리기 행동은 추가하지 않았습니다.

## 범위

천·가죽·금속 등의 재질 노드가 포함됩니다. 출력 PNG는 Workbench 스튜디오 렌더링이며 미세 표면 노드 효과는 Eevee/Cycles에서 확인할 수 있습니다. 의상 물리와 손가락 개별 리깅은 포함하지 않습니다.
게임 빌드·실행·테스트는 수행하지 않습니다. Blender 에셋 출력과 이미지 확인만 진행합니다.
