## 커밋 메시지 작성 규칙

GitHub Desktop의 "Generate commit message with Copilot"을 포함하여, 커밋 메시지를 생성할 때 다음 양식을 적용한다. 기준은 `.agents/skills/git-commit-push/SKILL.md`의 `커밋 메시지` 절이며, 아래 규칙과 예시는 그 양식을 따른다.

### 제목 (Summary)

- 반드시 `<유형>: <한국어 요약>` 형식의 한 줄로 작성한다. 영문 UI나 기존 영문 커밋에 관계없이 요약은 한국어로 작성한다.
- 유형은 `Add`, `Update`, `Fix`, `Refactor`, `Optimize`, `Remove`, `Docs`, `Test`, `Build`, `Chore` 중 주된 변경 목적에 맞게 하나를 선택하고 대소문자를 유지한다.
- 핵심 변경을 간결한 한국어 명사형으로 요약한다. `Refactor`는 동작을 유지하는 코드 구조 개선에 사용한다.

### 설명 (Description)

- 설명을 생략하지 않는다. 한국어 항목을 먼저 작성하고, 빈 줄 뒤에 같은 의미의 영어 항목을 작성한다. `한국어:`, `English:` 같은 언어 제목은 붙이지 않는다.
- 각 언어당 핵심 변경 1~3개를 `- ` 목록으로 작성한다. 두 언어의 항목 수와 순서를 맞춘다.
- 제목과 본문을 합칠 때에는 제목 뒤에 빈 줄을 하나 둔다. 실제 줄바꿈을 사용한다.
- GitHub Desktop의 Summary에는 제목 한 줄만, Description에는 한국어·영어 항목 전체를 넣는다. Description에 제목을 반복하지 않는다.
- 클래스명, 함수명, 파일명, API 이름 등 코드 식별자는 원문을 유지한다.
- 선택한 변경 사항만 요약한다. 구현하지 않은 기능이나 실행하지 않은 검사·테스트 결과를 추가하지 않는다.
- 생성 결과에 `Summary:`·`Description:` 라벨, 안내 문구, 전체 메시지를 감싸는 코드 펜스를 추가하지 않는다.

### 형식 예시

아래는 형식 예시이며, 실제 선택된 변경에 맞춰 내용을 작성한다.

```text
Fix: 피격 방향 계산 기준 수정

- 피격 방향 계산 기준을 무기 액터에서 피격 캐릭터로 변경
- 좌우 피격 몽타주 연결 수정

- Calculate hit direction relative to the hit character instead of the weapon actor
- Correct the left and right hit reaction montage assignments
```
