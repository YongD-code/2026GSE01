// Included inside Prototype: click navigation, skill state and procedural animation.
struct SkillEffect
{
    WorldPoint origin, target;
    int set = 0, slot = 0;
    float age = 0, duration = 1, nextPulse = .22f;
    bool applied = false;
    unsigned long long id = 0;
    WorldPoint position = {0, 0};
    int stage = 0;
    std::vector<WorldPoint> struck;
    std::vector<WorldPoint> visualTrail, impacts;
    float impactAge = -1;
    int caster = -1;
};

std::vector<WorldPoint> moveRoute;
size_t routeIndex = 0;
WorldPoint moveDestination = {0, 0};
bool hasMoveDestination = false, moveRouteComplete = false;
float navigationWait = 0, movementBlockedTime = 0;
int navigationRecoveries = 0;
int cursorX = 640, cursorY = 400;
bool attackHeld = false;

struct BufferedCommand
{
    int action = -1, set = 0, companion = -1;
    WorldPoint target = {0, 0};
    float remaining = 0;
} bufferedCommand;

bool replayingCommand = false;

void BufferCommand(int action)
{
    if (!replayingCommand && ControlsActive())
    {
        bufferedCommand = {action, SkillSet(), selectedCompanion, CursorWorld(), .3f};
    }
}

void MoveTo(WorldPoint target)
{
    attackHeld = false;
    bufferedCommand.action = -1;
    moveDestination = target;
    hasMoveDestination = true;
    moveRouteComplete = false;
    moveRoute.clear();
    routeIndex = 0;
    movementBlockedTime = 0;
    navigationRecoveries = 0;
    // Repeated clicks replace the destination without bypassing the search rate limit.
}

void StopMovement()
{
    hasMoveDestination = false;
    moveRouteComplete = false;
    moveRoute.clear();
    routeIndex = 0;
    movementVelocity = {0, 0};
    navigationWait = movementBlockedTime = 0;
    navigationRecoveries = 0;
}

void RetryMovement()
{
    moveRoute.clear();
    routeIndex = 0;
    movementVelocity = {0, 0};
    movementBlockedTime = 0;
    moveRouteComplete = false;
    if (++navigationRecoveries >= 3)
    {
        StopMovement();
        Say("이동", "해당 위치로 이동할 수 없습니다.");
        return;
    }
    navigationWait = (std::max)(navigationWait, .18f);
}

void UpdateMoveRoute(float dt)
{
    if (!hasMoveDestination || routeIndex < moveRoute.size() || navigationWait > 0 || dt <= 0)
    {
        return;
    }
    const WorldPoint actor = controlling ? spirit : player;
    if (moveRouteComplete || Distance(actor, moveDestination) < 2)
    {
        StopMovement();
        return;
    }
    moveRoute = ClickNavigation::Find(
        actor,
        moveDestination,
        [this](WorldPoint p)
        {
            return Blocked(p);
        },
        768,
        2.0,
        true,
        &moveDestination,
        &moveRouteComplete);
    routeIndex = 0;
    navigationWait = .12f;
    if (moveRoute.empty())
    {
        RetryMovement();
    }
}

void UpdateBufferedCommand(float dt)
{
    if (!ControlsActive() || bufferedCommand.set != SkillSet() ||
        bufferedCommand.companion != selectedCompanion)
    {
        bufferedCommand.action = -1;
    }
    if (bufferedCommand.action < 0)
    {
        return;
    }
    // Preparation, hit stop and dodging must not consume the acceptance window.
    if (PreparingSkill() || levelOne.dodgeTime > 0 || levelOne.hitStop > 0)
    {
        return;
    }
    const int action = bufferedCommand.action;
    if ((action < 4 && skillTimers[SkillSet()][action] > 0) ||
        (action == 4 && levelOne.cooldown > 0))
    {
        bufferedCommand.remaining -= dt;
        if (bufferedCommand.remaining <= 0)
        {
            bufferedCommand.action = -1;
        }
        return;
    }
    replayingCommand = true;
    if (action < 4)
        CastSkill(action);
    else if (action == 4)
        BasicAttack();
    replayingCommand = false;
    bufferedCommand.action = -1;
}

float skillTimers[SpiritCatalog::Count + 1][4] = {};
float spiritHasteTime = 0;
float spiritCastPose = 0;
Facing spiritCastFacing = Facing::South;
std::vector<SkillEffect> skillEffects;
unsigned long long nextSkillId = 1;
static constexpr float PurpleCharge = .72f;
static constexpr float PurpleDuration = 1.65f;
float purpleShake = 0;
static constexpr float DomainOpening = 1.15f;
static constexpr float DomainActive = 4.f;
static constexpr float DomainFade = .55f;
static constexpr float DomainRadius = 600.f;
float domainShake = 0;
static constexpr float RedCharge = .4f, RedFlight = .18f, RedFade = .95f;
static constexpr float BlueCharge = .4f, BlueActive = 2.5f, BlueFade = .45f;
static constexpr float RedRadius = 105.f, BlueRadius = 145.f;

bool InsideDomain(WorldPoint at) const
{
    for (const auto& effect : skillEffects)
    {
        if (effect.set == 0 && effect.slot == 3 && effect.age >= DomainOpening &&
            effect.age < DomainOpening + DomainActive && Distance(at, effect.origin) < DomainRadius)
        {
            return true;
        }
    }
    return false;
}

float DomainTimeScale(WorldPoint at, int kind) const
{
    if (!InsideDomain(at))
    {
        return 1.f;
    }
    return kind == 0 ? 0.f : kind == 1 ? .1f : .2f;
}

bool ChargingPurple() const
{
    return std::any_of(skillEffects.begin(),
                       skillEffects.end(),
                       [](const SkillEffect& e)
                       {
                           return e.set == 0 && e.slot == 0 && e.age < PurpleCharge;
                       });
}

bool PreparingSkill() const
{
    return ChargingPurple() ||
           std::any_of(skillEffects.begin(),
                       skillEffects.end(),
                       [](const SkillEffect& e)
                       {
                           return (e.set == 0 && ((e.slot == 3 && e.age < DomainOpening) ||
                                                  (e.slot == 1 && e.age < RedCharge) ||
                                                  (e.slot == 2 && e.age < BlueCharge))) ||
                                  (e.set > 0 && e.slot == 3 && e.age < DomainOpening) ||
                                  (e.set > 0 && e.slot == 0 &&
                                   e.age < (e.set == 2   ? .18f
                                            : e.set == 4 ? .55f
                                                         : .45f));
                       });
}

void ResetControls(bool restoring = false)
{
    talkingResident = -1;
    serviceNpc.clear();
    StopMovement();
    attackHeld = false;
    bufferedCommand.action = -1;
    spiritHasteTime = 0;
    spiritCastPose = 0;
    purpleShake = 0;
    domainShake = 0;
    skillEffects.clear();
    levelOne.hitFeedback.clear();
    levelOne.hitStop = levelOne.hitStopGuard = 0;
    ResetCharacterPresentation();
    for (auto& set : skillTimers)
    {
        for (float& timer : set)
        {
            timer = restoring ? levelOne.skillCooldown : 0;
        }
    }
}

float CharacterFacing(Facing facing) const
{
    const float angle = static_cast<int>(facing) * .785398163f;
    const double sx = -std::sin(angle), sy = std::cos(angle);
    return static_cast<float>(std::atan2((-sx / .85 + sy / .43) * .5, (sx / .85 + sy / .43) * .5) +
                              1.570796327);
}

void ResetCharacterPresentation()
{
    protagonistYaw = CharacterFacing(protagonistAnimation.facing);
    protagonistGait = protagonistModelTime = 0;
    protagonistCastAge = 1;
    protagonistModel.Reset(protagonistYaw);
}

void ToggleProtagonistModel()
{
    if (!protagonistModel.Ready())
    {
        Say("주인공 외형", "입체 모델을 읽지 못했습니다. 기존 외형을 사용합니다.");
        return;
    }
    useProtagonistModel = !useProtagonistModel;
    FrameProfiler::Event("character_render_mode", useProtagonistModel ? "mesh" : "sprite", 0);
    Say("주인공 외형",
        useProtagonistModel ? "입체 모델로 전환했습니다. M으로 비교할 수 있습니다."
                            : "기존 외형으로 전환했습니다. M으로 비교할 수 있습니다.");
}

void ToggleSpiritModels()
{
    bool ready = false;
    for (int species = 0; species < SpiritCatalog::Count; ++species)
    {
        ready = ready || spiritMeshes[species].Ready() || spiritModelSheets[species].Ready();
    }
    if (!ready)
    {
        Say("주령 외형", "입체 외형을 읽지 못했습니다. 기존 원화를 사용합니다.");
        return;
    }
    useSpiritModels = !useSpiritModels;
    FrameProfiler::Event("spirit_render_mode", useSpiritModels ? "models" : "original", 0);
    Say("주령 외형",
        useSpiritModels ? "주령들의 입체 외형으로 전환했습니다."
                        : "기존 원화로 전환했습니다. ESC 전투 메뉴에서 다시 바꿀 수 있습니다.");
}

void UpdateCharacterPresentation(float dt, double dx, double dy, double screenDistance)
{
    if (pauseMenu || journal || !serviceNpc.empty())
        return;
    protagonistModelTime = std::fmod(protagonistModelTime + dt, 16.f);
    if (chapter.choicePending || dialoguePending)
    {
        protagonistModel.Animate(
            dt, CharacterModel::Motion::Idle, protagonistModelTime * .25f, protagonistYaw);
        return;
    }
    protagonistCastAge += dt;
    const bool casting =
        !controlling && (PreparingSkill() || (levelOne.castTime > 0 && screenDistance < .001));
    if (screenDistance > .001 && !casting)
        protagonistYaw = static_cast<float>(std::atan2(dy, dx) + 1.570796327);
    protagonistGait = std::fmod(protagonistGait + static_cast<float>(screenDistance) /
                                                      (protagonistSprinting ? 68.f : 48.f),
                                1.f);
    CharacterModel::Motion motion = CharacterModel::Motion::Idle;
    float phase = protagonistModelTime * .25f;
    if (casting)
    {
        motion = CharacterModel::Motion::Cast;
        phase = PreparingSkill() ? (std::min)(.55f, protagonistCastAge / .32f)
                                 : (std::min)(1.f, protagonistCastAge / .32f);
    }
    else if (protagonistAnimation.moving)
    {
        motion = protagonistSprinting ? CharacterModel::Motion::Run : CharacterModel::Motion::Walk;
        phase = protagonistGait;
    }
    protagonistModel.Animate(dt, motion, phase, protagonistYaw);
}

bool ControlsActive() const
{
    return serviceNpc.empty() && !titleScreen && !openingPlaying && slotMode == SlotMode::Closed &&
           !pauseMenu && !journal && !chapter.choicePending && !dialoguePending && !levelOne.Dead();
}

int SkillSet() const
{
    return captured && controlling ? 1 + SpiritCatalog::Get(capturedSpirit).skillSpecies : 0;
}

const char* SkillName(int set, int slot) const
{
    static const char* names[5][4] = {
        {"허식 자", "술식반전 혁", "술식순전 창", "무량공처"},
        {"망각을 찢는 비명", "기억의 결박", "유체 이탈", "잊힌 자들의 합창"},
        {"귀향의 윤환", "순례의 족쇄", "귀향의 걸음", "망자의 행렬"},
        {"벽력장", "결박의 손", "수호의 기도", "천수의 결계"},
        {"삼중 성흔", "침묵의 낙인", "성자의 은총", "종말의 종"}};
    return names[set][slot];
}

WorldPoint CursorWorld() const
{
    if (replayingCommand)
    {
        return bufferedCommand.target;
    }
    const Point origin = Screen(camera.x, camera.y);
    const double x = (cursorX - origin.x) / .85, y = (cursorY - origin.y) / .43;
    return {camera.x + (x + y) * .5, camera.y + (y - x) * .5};
}

const char* SkillDescription(int set, int slot) const
{
    static const char* descriptions[5][4] = {
        {"주력을 모아 커서 방향으로 허식 자 방출",
         "붉은 주력 압축·발사 후 폭발과 밀치기",
         "푸른 구체로 2.5초간 적 흡인·지속 피해",
         "거대 영역 · 본체 가속 / 일반 적 정지 / 보스·투사체 감속"},
        {"0.45초 응축 후 전방 320 범위에 부채꼴 비명 피해",
         "원혼이 지정 범위를 붙잡아 피해·1.5초 결박",
         "2초 가속과 짧은 무적",
         "원혼들이 합창하는 영역에 지속 피해"},
        {"윤환을 던져 왕복 경로에 각각 피해·둔화",
         "지정 범위 피해와 4초 둔화",
         "2초 가속과 짧은 무적",
         "행렬이 전진하며 경로의 적에게 지속 피해"},
        {"0.45초 준비 후 전방 부채꼴 피해·밀치기",
         "지정 범위 피해와 1.5초 결박",
         "본체 체력 회복과 1.5초 보호",
         "조종 주령 중심 결계로 지속 피해·보호"},
        {"지정 지점의 문양 3개가 순차 폭발·둔화",
         "지정 범위 피해와 3초 둔화",
         "본체와 조종 중인 성자의 체력 25 회복",
         "종의 파동이 지정 영역에 지속 피해"}};
    return descriptions[set][slot];
}

void FaceCast(WorldPoint origin, WorldPoint target)
{
    SpriteAnimation aim;
    const double dx = target.x - origin.x, dy = target.y - origin.y;
    aim.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
    if (controlling)
    {
        spiritCastPose = .55f;
        spiritCastFacing = aim.facing;
    }
    else
    {
        protagonistCastAge = 0;
        protagonistYaw = static_cast<float>(std::atan2(dy, dx) + 1.570796327);
        levelOne.castFacing = aim.facing;
        levelOne.castTime = .18f;
    }
}

WorldPoint SkillAreaCenter(const SkillEffect& effect, bool activeClock = false) const
{
    if (effect.set == 2 && effect.slot == 3)
    {
        const float opening = activeClock ? 0.f : DomainOpening;
        const double travel =
            (std::max)(0.f, (std::min)(1.f, (effect.age - opening) / (effect.duration - opening)));
        return {effect.origin.x + (effect.target.x - effect.origin.x) * travel,
                effect.origin.y + (effect.target.y - effect.origin.y) * travel};
    }
    if ((effect.set == 0 || effect.set == 3) && effect.slot == 3)
    {
        return effect.origin;
    }
    return effect.target;
}

void BasicAttack(bool buffer = false)
{
    if (!ControlsActive() || PreparingSkill() || levelOne.cooldown > 0 || levelOne.dodgeTime > 0 ||
        levelOne.hitStop > 0)
    {
        if (buffer)
            BufferCommand(4);
        return;
    }
    if (!replayingCommand)
        bufferedCommand.action = -1;
    const WorldPoint origin = controlling ? spirit : player;
    WorldPoint target = CursorWorld();
    double nearest = 1.e12;
    for (const auto& enemy : levelOne.enemies)
    {
        if (replayingCommand)
            break;
        if (enemy.health <= 0 || enemy.weakened || enemy.returning)
        {
            continue;
        }
        const Point feet = Screen(enemy.position.x, enemy.position.y);
        const float spriteHeight = enemy.boss > 0 ? 100.f : 68.f;
        if (std::abs(cursorX - feet.x) > 34 || cursorY < feet.y - spriteHeight ||
            cursorY > feet.y + 8)
        {
            continue;
        }
        const double distance =
            std::hypot(cursorX - feet.x, cursorY - (feet.y - spriteHeight * .5));
        if (distance < nearest)
        {
            nearest = distance;
            target = enemy.position;
        }
    }
    const double distance = Distance(origin, target);
    if (distance < 1)
    {
        return;
    }
    levelOne.projectiles.push_back(
        {origin,
         {(target.x - origin.x) / distance * 440, (target.y - origin.y) / distance * 440},
         levelOne.Range(),
         levelOne.Damage(),
         false});
    levelOne.cooldown = levelOne.Interval();
    FaceCast(origin, target);
}

void CastSkill(int slot)
{
    if (!ControlsActive())
    {
        return;
    }
    if (chapter.step < chapter.Index("P005"))
    {
        Say("술식", "아직 주력을 다루는 법을 모릅니다. 묘지에서 라엔을 만나세요.");
        return;
    }
    const int set = SkillSet();
    // Do not reserve unavailable skills for an entire cooldown, even during another cast.
    if (skillTimers[set][slot] > .3f)
    {
        return;
    }
    if (PreparingSkill() || levelOne.dodgeTime > 0 || levelOne.hitStop > 0 ||
        skillTimers[set][slot] > 0)
    {
        BufferCommand(slot);
        return;
    }
    if (!replayingCommand)
        bufferedCommand.action = -1;
    if (set > 0)
    {
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.boss == 3 && enemy.alerted && enemy.health > 0)
            {
                Say("술식 봉쇄", "눈먼 수도사가 주령 능력을 봉쇄하고 있습니다.");
                return;
            }
        }
    }
    const WorldPoint origin = controlling ? spirit : player;
    WorldPoint target = CursorWorld();
    double distance = Distance(origin, target);
    if (distance < 1)
    {
        target.x += 1;
        distance = 1;
    }
    const double reach = slot == 0 ? (set == 3 ? 175 : set == 2 ? 320 : 420) : 260;
    const double length = slot == 0 && set != 1 && set != 4 ? reach : (std::min)(distance, reach);
    target = {origin.x + (target.x - origin.x) / distance * length,
              origin.y + (target.y - origin.y) / distance * length};
    static const float waits[4] = {6, 9, 12, 40};
    skillTimers[set][slot] = waits[slot];
    levelOne.skillCooldown = (std::max)(levelOne.skillCooldown, waits[slot]);
    const auto* quest = chapter.Current();
    if (set > 0 && (!quest || quest->trigger != "skill" || (set == 1 && slot == quest->amount)))
    {
        ++levelOne.skillsUsed;
    }
    // Retain the destination and safe route so movement resumes after preparation.
    movementVelocity = {0, 0};
    attackHeld = false;
    FaceCast(origin, target);
    if (set == 0 && (slot == 1 || slot == 2))
    {
        // Stop at the last unobstructed point so the focus cannot appear inside a wall.
        const int steps = (std::max)(1, int(std::ceil(Distance(origin, target) / 6)));
        WorldPoint safe = origin;
        for (int i = 1; i <= steps; ++i)
        {
            const double t = double(i) / steps;
            const WorldPoint at = {origin.x + (target.x - origin.x) * t,
                                   origin.y + (target.y - origin.y) * t};
            if (Blocked(at))
            {
                break;
            }
            safe = at;
        }
        target = safe;
    }
    skillEffects.push_back(
        {origin, target, set, slot, 0, slot == 3 ? 4.f : .85f, .22f, false, nextSkillId++});
    if (set > 0)
    {
        auto& effect = skillEffects.back();
        effect.caster = selectedCompanion;
        if (slot == 3)
        {
            effect.duration += DomainOpening;
            effect.nextPulse = DomainOpening;
        }
        if (slot == 1)
        {
            effect.duration = .22f + (set == 2 ? 4.f : set == 4 ? 3.f : 1.5f);
        }
        if (slot == 2)
        {
            effect.duration = set <= 2 ? 2.f : set == 3 ? 1.5f : 1.f;
        }
    }
    if (set > 0 && slot == 0)
    {
        auto& effect = skillEffects.back();
        effect.position = origin;
        effect.duration = set == 1 ? 1.25f : set == 2 ? 1.65f : set == 3 ? .9f : 1.5f;
        effect.nextPulse = set == 3 ? .45f : .55f;
        skillTimers[set][slot] = set == 1 ? 5.f : set == 2 ? 7.f : set == 3 ? 8.f : 9.f;
        spiritCastPose = set == 3 ? .8f : .55f;
    }
    if (set == 0 && slot == 0)
    {
        skillEffects.back().duration = PurpleDuration;
        skillEffects.back().nextPulse = PurpleCharge;
    }
    if (set == 0 && (slot == 1 || slot == 2))
    {
        skillEffects.back().duration =
            slot == 1 ? RedCharge + RedFlight + RedFade : BlueCharge + BlueActive + BlueFade;
        skillEffects.back().nextPulse = slot == 1 ? RedCharge + RedFlight : BlueCharge;
    }
    if (set == 0 && slot == 3)
    {
        skillEffects.back().target = origin;
        skillEffects.back().duration = DomainOpening + DomainActive + DomainFade;
        skillEffects.back().nextPulse = DomainOpening;
        domainShake = .10f;
    }
    if (set > 0 && slot == 2)
    {
        spiritHasteTime = set <= 2 ? 2.f : 0.f;
        if (set == 4)
        {
            levelOne.health = (std::min)(levelOne.MaximumHealth(), levelOne.health + 25);
            if (selectedCompanion >= 0 && selectedCompanion < int(levelOne.collection.size()))
            {
                auto& companion = levelOne.collection[selectedCompanion];
                companion.health = (std::min)(100.f, companion.health + 25);
            }
        }
        else
        {
            if (set == 3)
            {
                levelOne.health = (std::min)(levelOne.MaximumHealth(), levelOne.health + 12.f);
            }
            levelOne.spiritInvulnerability = set == 3 ? 1.5f : .65f;
        }
    }
}

#include "PrototypeRed.inl"
#include "PrototypePolarity.inl"
#include "PrototypeSpiritSkills.inl"
#include "PrototypeSpiritEffects.inl"

void UpdateSkills(float dt)
{
    spiritCastPose = (std::max)(0.f, spiritCastPose - dt);
    purpleShake = (std::max)(0.f, purpleShake - dt);
    domainShake = (std::max)(0.f, domainShake - dt);
    levelOne.skillCooldown = 0;
    for (auto& set : skillTimers)
    {
        for (float& timer : set)
        {
            timer = (std::max)(0.f, timer - dt);
            levelOne.skillCooldown = (std::max)(levelOne.skillCooldown, timer);
        }
    }
    spiritHasteTime = (std::max)(0.f, spiritHasteTime - dt);
    for (auto& effect : skillEffects)
    {
        effect.age += dt;
        if (effect.set > 0 && effect.slot == 3)
        {
            if (effect.age < DomainOpening)
            {
                spiritCastPose = .14f;
                continue;
            }
            if (!effect.applied && effect.set == 3)
            {
                levelOne.spiritInvulnerability = (std::max)(levelOne.spiritInvulnerability, 2.f);
            }
            if (!effect.applied)
            {
                domainShake = (std::max)(domainShake, .14f);
            }
        }
        if (effect.impactAge >= 0)
        {
            effect.impactAge += dt;
        }
        if (effect.set > 0 && (effect.slot == 0 || effect.slot == 2))
        {
            const WorldPoint at =
                effect.slot == 2
                    ? (effect.caster == selectedCompanion && captured ? spirit : effect.origin)
                : effect.set == 2 ? SpiritQPosition(effect)
                                  : effect.position;
            if (effect.visualTrail.empty() || Distance(effect.visualTrail.back(), at) > 7)
            {
                effect.visualTrail.push_back(at);
                if (effect.visualTrail.size() > 12)
                {
                    effect.visualTrail.erase(effect.visualTrail.begin());
                }
            }
        }
        if (effect.set > 0 && effect.slot == 0)
        {
            UpdateSpiritQ(effect, dt);
            continue;
        }
        if (effect.set == 0 && (effect.slot == 1 || effect.slot == 2))
        {
            UpdatePolarity(effect, dt);
            continue;
        }
        if (effect.set == 0 && effect.slot == 3)
        {
            if (effect.age < DomainOpening)
            {
                levelOne.castTime = .14f;
                domainShake = .06f + .04f * effect.age / DomainOpening;
            }
            else if (!effect.applied)
            {
                domainShake = .24f;
                levelOne.castTime = .09f;
            }
            if (effect.age >= DomainOpening + DomainActive)
            {
                continue;
            }
        }
        if (effect.set == 0 && effect.slot == 0)
        {
            if (effect.age < PurpleCharge)
                levelOne.castTime = .14f;
            else if (!effect.applied)
            {
                purpleShake = .22f;
                levelOne.castTime = .09f;
            }
        }
        if (effect.age < effect.nextPulse || (effect.applied && effect.slot != 3))
        {
            continue;
        }
        effect.applied = true;
        effect.nextPulse += .5f;
        effect.impacts.clear();
        const WorldPoint areaCenter = SkillAreaCenter(effect);
        for (auto& enemy : levelOne.enemies)
        {
            if (enemy.health <= 0 || enemy.weakened || enemy.returning)
            {
                continue;
            }
            const double radius = effect.slot == 3
                                      ? (effect.set == 0   ? DomainRadius
                                         : effect.set == 2 ? 105
                                                           : 180)
                                      : (effect.set == 1 && effect.slot == 0 ? 28 : 105);
            bool hit = Distance(enemy.position, areaCenter) < radius;
            if (effect.slot == 0 && effect.set != 1)
            {
                const double vx = effect.target.x - effect.origin.x,
                             vy = effect.target.y - effect.origin.y;
                const double squared = vx * vx + vy * vy;
                const double t =
                    squared > 0
                        ? (std::max)(0.0,
                                     (std::min)(1.0,
                                                ((enemy.position.x - effect.origin.x) * vx +
                                                 (enemy.position.y - effect.origin.y) * vy) /
                                                    squared))
                        : 0;
                hit = Distance(enemy.position,
                               {effect.origin.x + vx * t, effect.origin.y + vy * t}) < 38;
            }
            if (effect.set > 0 && effect.slot == 2)
            {
                continue;
            }
            if (!hit || !LevelOne::ClearLine(effect.origin,
                                             enemy.position,
                                             [this](WorldPoint p)
                                             {
                                                 return Blocked(p);
                                             }))
            {
                continue;
            }
            const float factor = effect.slot == 0 ? 2.5f : (effect.slot == 3 ? .45f : 1.2f);
            levelOne.DamageEnemy(enemy,
                                 levelOne.Damage() * factor,
                                 effect.origin,
                                 effect.slot == 0,
                                 effect.slot != 3);
            if (effect.set > 0 && effect.impacts.size() < 8)
            {
                effect.impacts.push_back(enemy.position);
                effect.impactAge = 0;
            }
            if (enemy.weakened || enemy.health <= 0 || enemy.boss == 12)
            {
                continue;
            }
            if (effect.set == 2 && (effect.slot == 0 || effect.slot == 1))
            {
                enemy.slowed = (std::max)(enemy.slowed, effect.slot == 1 ? 4.f : 1.5f);
            }
            if (effect.set == 2 && effect.slot == 3)
            {
                enemy.slowed = (std::max)(enemy.slowed, 1.f);
            }
            if (effect.set == 4 && effect.slot == 1)
            {
                enemy.slowed = 3;
            }
            if ((effect.set == 1 && effect.slot == 1) || (effect.set == 3 && effect.slot == 1))
            {
                enemy.stagger = (std::max)(enemy.stagger, effect.slot == 3 ? .6f : 1.5f);
                enemy.phase = 0;
            }
            if (effect.set == 3 && effect.slot == 3)
            {
                enemy.stagger = (std::max)(enemy.stagger, .55f);
                enemy.phase = 0;
            }
            if (effect.set == 3 && effect.slot == 3)
            {
                const WorldPoint center = effect.origin;
                double dx = enemy.position.x - center.x, dy = enemy.position.y - center.y;
                const double d = (std::max)(1.0, std::hypot(dx, dy));
                const double travel = 28;
                LevelOne::Move(
                    enemy.position,
                    {dx / d * travel, dy / d * travel},
                    [this](WorldPoint p)
                    {
                        return Blocked(p);
                    },
                    &enemy.trail);
            }
        }
    }
    skillEffects.erase(std::remove_if(skillEffects.begin(),
                                      skillEffects.end(),
                                      [](const SkillEffect& e)
                                      {
                                          return e.age >= e.duration;
                                      }),
                       skillEffects.end());
}

UIRect SkillBarBounds() const
{
    const float barWidth = (std::min)(520.f, float(width) - 40);
    return {(float(width) - barWidth) * .5f, float(height) - 76, barWidth, 48};
}

void DrawSkillBar()
{
    const auto bounds = SkillBarBounds();
    const int set = SkillSet();
    const bool locked = chapter.step < chapter.Index("P005");
    const char* keys = "QWER";
    for (int i = 0; i < 4; ++i)
    {
        const float stride = bounds.w / 4;
        const UIRect cell = {bounds.x + i * stride, bounds.y, stride - 4, bounds.h};
        Panel(cell.x, cell.y, cell.w, cell.h);
        std::string label(1, keys[i]);
        label += locked ? " 잠김"
                 : skillTimers[set][i] > 0
                     ? " " + std::to_string(int(std::ceil(skillTimers[set][i]))) + "초"
                     : " 준비";
        r.Text(
            cell.x + 7, cell.y + 17, label, locked ? Color(.55f, .58f, .64f) : Color(.7f, .85f, 1));
        r.Text(cell.x + 5, cell.y + 37, SkillName(set, i), Color(.9f, .9f, .85f));
    }
}

void DrawPurpleEffect(const SkillEffect& effect)
{
    const Point origin = Screen(effect.origin.x, effect.origin.y, 35);
    const Point end = Screen(effect.target.x, effect.target.y, 35);
    const float dx = end.x - origin.x, dy = end.y - origin.y;
    const float length = (std::max)(1.f, std::hypot(dx, dy));
    const float ux = dx / length, uy = dy / length;
    const Point focus = {origin.x + ux * 22, origin.y + uy * 22};
    auto ring = [&](Point p, float radius, float flatten, float thickness, Color color)
    {
        // Fixed segment and particle budgets use the existing instanced renderer.
        for (int i = 0; i < 32; ++i)
        {
            const float a = i * 6.2831853f / 32, b = (i + 1) * 6.2831853f / 32;
            r.Line({p.x + std::cos(a) * radius, p.y + std::sin(a) * radius * flatten},
                   {p.x + std::cos(b) * radius, p.y + std::sin(b) * radius * flatten},
                   thickness,
                   color);
        }
    };
    auto glow = [&](Point p, float radius, Color color, float alpha)
    {
        for (int layer = 3; layer >= 1; --layer)
            r.Ellipse(p.x,
                      p.y,
                      radius * (1 + layer * .35f),
                      radius * (1 + layer * .35f),
                      Color(color.r, color.g, color.b, alpha * .07f));
        r.Ellipse(p.x, p.y, radius, radius, Color(color.r, color.g, color.b, alpha * .85f));
        r.Ellipse(p.x, p.y, radius * .42f, radius * .42f, Color(1.8f, 1.5f, 2.f, alpha));
    };
    if (effect.age < PurpleCharge)
    {
        const float t = effect.age / PurpleCharge;
        const float ease = t * t * (3 - 2 * t);
        const float orbit = (1 - ease) * 44;
        const float angle = t * 5;
        const Point red = {focus.x + std::cos(angle) * orbit,
                           focus.y + std::sin(angle) * orbit * .55f};
        const Point blue = {focus.x - std::cos(angle) * orbit,
                            focus.y - std::sin(angle) * orbit * .55f};
        ring(Screen(effect.origin.x, effect.origin.y),
             28 + ease * 45,
             .43f,
             2,
             Color(.65f, .25f, 1.2f, .2f + t * .3f));
        for (int i = 0; i < 16; ++i)
        {
            const float a = i * 2.39996f + t * 1.5f;
            const float phase = std::fmod(t * 1.7f + i * .173f, 1.f);
            const float radius = 15 + (1 - phase) * 70;
            const Point p = {focus.x + std::cos(a) * radius, focus.y + std::sin(a) * radius * .7f};
            r.Line(p,
                   {p.x - std::cos(a) * 10, p.y - std::sin(a) * 7},
                   1.5f,
                   Color(.65f, .4f, 1.3f, phase * .65f));
        }
        glow(red, 7 + t * 5, Color(1.5f, .08f, .2f), .9f);
        glow(blue, 7 + t * 5, Color(.1f, .45f, 1.8f), .9f);
        glow(focus, 4 + ease * 15, Color(.85f, .2f, 1.7f), ease);
        return;
    }
    const float elapsed = effect.age - PurpleCharge;
    const float fade = (std::max)(0.f, 1 - elapsed / (PurpleDuration - PurpleCharge));
    const float blast = std::exp(-elapsed * 7);
    // The full corridor lights up on the same update as its single damage pulse.
    r.Line(focus, end, 58 * blast + 15 * fade, Color(.5f, .08f, 1.2f, fade * .18f));
    r.Line(focus, end, 29 * blast + 8 * fade, Color(.85f, .2f, 1.8f, fade * .65f));
    r.Line(focus, end, 13 * blast + 2 * fade, Color(1.8f, 1.35f, 2.f, fade * .95f));
    glow(focus, 20 * blast + 4 * fade, Color(.85f, .2f, 1.6f), fade);
    glow(end, 30 * blast + 8 * fade, Color(.75f, .12f, 1.7f), fade);
    ring(end, 18 + elapsed * 105, .65f, 3 * fade + .5f, Color(.9f, .35f, 1.5f, fade * .8f));
    ring(focus, 14 + elapsed * 95, .48f, 2, Color(.7f, .3f, 1.4f, fade * .6f));
    for (int i = 0; i < 20; ++i)
    {
        const float t = (i + .5f) / 20;
        const float side = i % 2 ? 1.f : -1.f;
        const float drift = side * (10 + elapsed * (25 + i % 4 * 15));
        const Point p = {focus.x + (end.x - focus.x) * t - uy * drift,
                         focus.y + (end.y - focus.y) * t + ux * drift};
        r.Line(p,
               {p.x - ux * (6 + blast * 18), p.y - uy * (6 + blast * 18)},
               2 * fade + .5f,
               Color(.85f, .5f, 1.5f, fade * .75f));
    }
}

#include "PrototypeDomain.inl"

void DrawSkillEffect(const SkillEffect& effect)
{
    if (effect.set > 0)
    {
        if (effect.slot == 3)
        {
            if (effect.age < DomainOpening)
            {
                return;
            }
            SkillEffect active = effect;
            active.age -= DomainOpening;
            active.duration -= DomainOpening;
            DrawSpiritSkill(active);
        }
        else
        {
            DrawSpiritSkill(effect);
        }
    }
    else if (effect.slot == 0)
    {
        DrawPurpleEffect(effect);
    }
    else if (effect.slot == 3)
    {
        DrawDomain(effect, false);
    }
    else
    {
        DrawPolarity(effect);
    }
}
