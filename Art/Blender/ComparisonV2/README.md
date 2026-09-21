# NPC 외형 비교 후보

기존 게임 에셋과 `Art/Blender/npc1.blend`, `npc2.blend`는 변경하지 않았습니다.
이 폴더는 사용자가 외형을 선택하기 위한 별도 모델입니다. 게임 적용과 전체 애니메이션 시트 재출력은 아직 진행하지 않았습니다.

- `npc1_candidate.blend`, `npc2_candidate.blend`: 새 외형 후보. 기존 뼈대를 유지했지만 새 외형의 전체 이동 동작은 아직 확인하지 않았습니다.
- `npc1_before_after.png`, `npc2_before_after.png`: 왼쪽 기존 / 오른쪽 후보. 카메라 및 Workbench 조명 조건을 맞춘 비교입니다.
- `npc1_after_lit.png`, `npc2_after_lit.png`: 후보의 Cycles 조명·재질 미리보기. 조명이 다르므로 형상만 비교할 때는 before_after 이미지를 사용하세요.
- `create_candidates.py`: 기존 모델을 읽어 이 폴더에만 새 모델과 비교 이미지를 생성하는 Blender 스크립트.

변경: 눈 크기와 눈꺼풀, 얼굴에 이어지는 코, 소매 두께, 덩어리로 연결된 머리 형태와 얕은 머릿결, 별도 조명·재질 프리뷰.

게임 빌드·실행·테스트는 수행하지 않았습니다.
