# CLAUDE.md 관리 플러그인

CLAUDE.md 파일을 유지보수하고 개선하는 도구 모음 — 품질 감사, 세션 학습 반영, 프로젝트 메모리를 최신 상태로 유지합니다.

## 무엇을 하는가

목적이 다른 두 가지 도구로 구성됩니다.

| | claude-md-improver (스킬) | /revise-claude-md (커맨드) |
|---|---|---|
| **목적** | CLAUDE.md를 코드베이스와 동기화 | 세션 학습 내용 반영 |
| **트리거** | 코드베이스 변경 시 | 세션 종료 시 |
| **사용 시점** | 주기적 유지보수 | 세션에서 누락된 컨텍스트 발견 시 |

## 사용법

### 스킬: claude-md-improver

현재 코드베이스 상태와 비교해 CLAUDE.md 파일을 감사합니다.

```
"audit my CLAUDE.md files"
"check if my CLAUDE.md is up to date"
```

품질 점수(A~F)와 구체적인 개선 제안을 항목별로 출력합니다.

### 커맨드: /revise-claude-md

현재 세션에서 배운 내용을 CLAUDE.md에 반영합니다.

```
/revise-claude-md
```

세션 대화를 돌아보며 추가할 내용과 삭제할 오래된 항목을 diff 형태로 제안한 뒤, 승인 후 적용합니다.

## 작성자

Isabella He (isabella@anthropic.com)
