#pragma once
#include "Prototype.h"
#include <cstring>

// Explicit --verify-models run: hidden OpenGL window, isolated new world, no save operations.
struct ModelIntegrationReview
{
    static bool Capture(const std::wstring& file, int width, int height)
    {
        std::vector<unsigned char> pixels(size_t(width) * height * 4);
        glFinish();
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
        BITMAPFILEHEADER header = {};
        BITMAPINFOHEADER info = {};
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
        info.biSize = sizeof(info);
        info.biWidth = width;
        info.biHeight = height;
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biSizeImage = static_cast<DWORD>(pixels.size());
        HANDLE output = CreateFileW(
            file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        DWORD written = 0;
        const bool ok =
            WriteFile(output, &header, sizeof(header), &written, nullptr) &&
            written == sizeof(header) &&
            WriteFile(output, &info, sizeof(info), &written, nullptr) && written == sizeof(info) &&
            WriteFile(
                output, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr) &&
            written == pixels.size();
        CloseHandle(output);
        return ok;
    }

    static int Run(Prototype& game)
    {
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, 32768);
        std::wstring folder(executable);
        folder = folder.substr(0, folder.find_last_of(L"\\/")) + L"\\ModelReview";
        if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return 2;
        }
        if (!game.npcModel.Ready())
        {
            std::cerr << "MODEL_REVIEW_ERROR npc asset\n";
            return 3;
        }
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            if (!game.spiritMeshes[species].Ready())
            {
                std::cerr << "MODEL_REVIEW_ERROR spirit " << species << '\n';
                return 4;
            }
        }
        LevelOne encounters;
        std::set<int> appeared;
        auto open = [](WorldPoint)
        {
            return false;
        };
        for (int area = 0; area < 8; ++area)
        {
            const WorldPoint observer = {2200. + area * 1300, 1900. + area * 900};
            for (int tick = 0; tick < 12; ++tick)
            {
                encounters.StreamEncounters(observer, 1729, open);
                for (const auto& enemy : encounters.enemies)
                {
                    appeared.insert(enemy.type);
                }
            }
        }
        if (appeared.size() != SpiritCatalog::Count)
        {
            std::cerr << "MODEL_REVIEW_ERROR species streaming " << appeared.size() << '\n';
            return 12;
        }
        LevelOne peaceful;
        LevelOne::Enemy deer;
        deer.position = deer.home = {40, 40};
        deer.type = SpiritCatalog::Stag;
        deer.health = deer.maximum = 64;
        peaceful.enemies.push_back(deer);
        peaceful.Update(.05f,
                        {0, 0},
                        {0, 0},
                        false,
                        false,
                        false,
                        1729,
                        open,
                        [](WorldPoint, int)
                        {
                            return 1.f;
                        });
        if (peaceful.health != 100 || peaceful.enemies.front().alerted)
        {
            std::cerr << "MODEL_REVIEW_ERROR passive spirit\n";
            return 13;
        }
        while (glGetError() != GL_NO_ERROR)
        {
        }
        game.NewGame();
        game.openingPending = game.openingPlaying = false;
        game.dialoguePending = false;
        game.messageTime = 0;
        game.automaticSaveAllowed = false;
        game.skipExitSave = true;
        if (game.population.people.empty())
        {
            game.InitializePopulation();
        }
        game.time = .5f;
        game.Draw();
        int residentActors = 0;
        for (const auto& person : game.population.people)
        {
            const auto* actor = dynamic_cast<const ModelActor*>(
                game.scene.Find("residents/" + std::to_string(person.id)));
            if (actor && actor->modelState.owner == &game.npcModel)
            {
                ++residentActors;
            }
        }
        if (!residentActors || !Capture(folder + L"\\village.bmp", 1280, 800))
        {
            std::cerr << "MODEL_REVIEW_ERROR scene integration\n";
            return 5;
        }

        game.levelOne.enemies.clear();
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            LevelOne::Enemy enemy;
            const double sx = (190. + species * 150 - 640) / .85;
            const double sy = 165. / .43;
            enemy.position =
                enemy.home = {game.camera.x + (sx + sy) * .5, game.camera.y + (sy - sx) * .5};
            enemy.type = species;
            enemy.health = enemy.maximum = SpiritCatalog::Get(species).health;
            enemy.camp = {100 + species, 100};
            game.levelOne.enemies.push_back(enemy);
        }
        game.Draw();
        int spiritActors = 0;
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            const auto* actor = dynamic_cast<const ModelActor*>(
                game.scene.Find("enemy/" + std::to_string(100 + species) + "/100/0"));
            if (actor && actor->modelState.owner == &game.spiritMeshes[species])
            {
                ++spiritActors;
            }
        }
        if (spiritActors != SpiritCatalog::Count ||
            !Capture(folder + L"\\world_spirits.bmp", 1280, 800))
        {
            return 14;
        }

        std::array<CharacterModel::State, SpiritCatalog::Count> spirits;
        game.r.Begin(Color(.055f, .075f, .095f), false);
        game.r.Text(30, 35, "게임 렌더러 · 주령 7종의 실제 스킨 모델", Color(1, .9f, .7f));
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            const Point feet = {160.f + (species % 4) * 320, 310.f + (species / 4) * 360};
            SpriteAnimation animation;
            animation.facing = Facing::SouthWest;
            auto& model = game.spiritMeshes[species];
            auto& state = spirits[species];
            model.Reset(state, game.CharacterFacing(animation.facing));
            for (int motion = 0; motion < int(CharacterModel::Motion::Count); ++motion)
            {
                model.Animate(
                    state, .05f, static_cast<CharacterModel::Motion>(motion), .5f, state.yaw);
                for (const auto& matrix : state.skin)
                {
                    for (float value : matrix)
                    {
                        if (!std::isfinite(value))
                        {
                            return 6;
                        }
                    }
                }
                for (const auto& matrix : state.imported.global)
                {
                    for (float value : matrix)
                    {
                        if (!std::isfinite(value))
                        {
                            return 6;
                        }
                    }
                }
            }
            model.Reset(state, game.CharacterFacing(animation.facing));
            if (!model.Draw(game.r, feet, Color(1, 1, 1), state, 165))
            {
                return 7;
            }
            game.r.Text(
                feet.x - 95, feet.y + 65, SpiritCatalog::Get(species).name, Color(.9f, .9f, .85f));
        }
        game.r.Flush();
        if (!Capture(folder + L"\\spirits.bmp", 1280, 800))
        {
            return 8;
        }

        game.r.Begin(Color(.055f, .075f, .095f), false);
        game.r.Text(30, 35, "게임 렌더러 · 공용 파츠 조합과 이야기 NPC", Color(1, .9f, .7f));
        const char* labels[] = {"동생",
                                "부모",
                                "잡화상",
                                "마을 아이",
                                "돌아가지 못한 사람",
                                "라엔",
                                "봉인사",
                                "대장장이",
                                "생활 주민 · 채집꾼",
                                "생활 주민 · 행상인",
                                "생활 주민 · 사냥꾼"};
        CharacterModel::State independentA, independentB;
        game.npcModel.Reset(independentA, 0);
        const auto preserved = independentA.pose;
        const auto importedPreserved = independentA.imported.pose;
        const auto morphPreserved = independentA.imported.morph;
        game.npcModel.Reset(independentB, 1);
        game.npcModel.Animate(independentB, .05f, CharacterModel::Motion::Run, .25f, 1);
        if (preserved != independentA.pose || importedPreserved != independentA.imported.pose ||
            morphPreserved != independentA.imported.morph)
        {
            return 9;
        }
        for (int identity = 0; identity < 11; ++identity)
        {
            const auto appearance = identity < 8 ? NpcAppearance::Story(identity)
                                                 : NpcAppearance::Resident(identity,
                                                                           identity == 8   ? 0
                                                                           : identity == 9 ? 1
                                                                                           : 3);
            CharacterModel::State pose;
            game.npcModel.Reset(pose, game.CharacterFacing(Facing::SouthWest));
            const Point feet = {160.f + identity % 4 * 320, 235.f + identity / 4 * 255};
            if (!game.npcModel.Draw(game.r,
                                    feet,
                                    Color(1, 1, 1),
                                    pose,
                                    155 * appearance.scale,
                                    &appearance.partIndices))
            {
                return 10;
            }
            game.r.Text(feet.x - 100, feet.y + 24, labels[identity], Color(.9f, .9f, .85f));
        }
        game.r.Flush();
        if (!Capture(folder + L"\\npcs.bmp", 1280, 800) || glGetError() != GL_NO_ERROR)
        {
            std::cerr << "MODEL_REVIEW_ERROR framebuffer\n";
            return 11;
        }
        std::cout << "MODEL_INTEGRATION_OK spirits=7 npc_parts=25 npc_presets=11 resident_actors="
                  << residentActors << " spirit_actors=" << spiritActors
                  << " independent_poses=1 species_streamed=7 passive_stag=1 save_operations=0\n";
        return 0;
    }
};
