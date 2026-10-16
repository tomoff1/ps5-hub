// PS5 Hub - Interface native française et navigation DualSense.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "concepts/concepts.hpp"
#include "hub/service.hpp"
#include "ui/components/list.hpp"
#include "ui/components/dialog.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/glyphs.hpp"
#include <ctime>

namespace hui::concepts
{
namespace
{
using gfx::Color;
constexpr const char *sections[] = {"Accueil", "Homebrew", "Applications", "Fichiers",
                                    "Paramètres"};
constexpr const char *subtitles[] = {"Votre espace homebrew, sur grand écran.",
                                     "Découvrez des projets libres et ajoutez vos archives.",
                                     "Vos liens personnels et vos paquets PKG.",
                                     "Les fichiers réellement présents dans votre stockage.",
                                     "Une interface à votre rythme."};
constexpr const char *techniques[] = {
    "Rendu OpenGL natif", "Téléchargements HTTPS sur un travailleur", "Firmware 13.60 non testé"};
#ifdef HUB_PS5
constexpr const char *data_root = "/download0/hub";
constexpr const char *assets_root = "/app0/assets";
#else
constexpr const char *data_root = "build/preview-data";
constexpr const char *assets_root = "assets";
#endif
ui::ListItem row(std::string title, std::string subtitle = {}, std::string badge = {})
{
    ui::ListItem r;
    r.title = std::move(title);
    r.subtitle = std::move(subtitle);
    r.badge = std::move(badge);
    r.chevron = true;
    return r;
}
class Hub final : public app::Concept
{
  public:
    explicit Hub(app::Context &c) : context_(c), service_(data_root, assets_root)
    {
        theme_ = ui::themes()[0];
        theme_.surface = Color::rgb(0x151e30);
        theme_.surface_high = Color::rgb(0x22314a);
        theme_.primary = theme_.accent = Color::rgb(0x80edd4);
        theme_.on_primary = Color::rgb(0x07151a);
        theme_.text = Color::rgb(0xf1f5fc);
        theme_.text_muted = Color::rgb(0xa6b7ce);
        theme_.focus = Color::rgb(0x80edd4);
        theme_.style = ui::SurfaceStyle::soft;
        nav_.style.theme = items_.style.theme = dialog_.style.theme = keys_.style.theme = theme_;
        nav_.set_bounds({96, 270, 340, 590});
        nav_.style.row_height = 82;
        std::vector<ui::ListItem> navigation;
        for (const auto *s : sections)
            navigation.push_back(row(s));
        nav_.set_items(std::move(navigation));
        items_.set_bounds({504, 432, 1320, 420});
        items_.style.row_height = 104;
        items_.style.cards = true;
        items_.style.gap = 14;
        items_.set_active(false);
        keys_.set_bounds({480, 470, 1344, 390});
        keys_.style.done_label = "Valider";
        keys_.style.space_label = "Espace";
        keys_.style.bindings = ui::KeyboardBindings::standard();
        keys_.style.max_length = 120;
        keys_.on_text = [this](std::string_view text)
        {
            edit_error_.clear();
            if (edit_.size() + text.size() <= static_cast<std::size_t>(keys_.style.max_length))
                edit_ += text;
        };
        keys_.on_backspace = [this]
        {
            edit_error_.clear();
            if (!edit_.empty())
            {
                auto pos = edit_.size() - 1;
                while (pos > 0 && (static_cast<unsigned char>(edit_[pos]) & 0xc0) == 0x80)
                    --pos;
                edit_.erase(pos);
            }
        };
        refresh();
        service_.load();
    }
    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{"hub",
                                           "PS5 Hub",
                                           "Votre bibliothèque homebrew",
                                           "src/concepts/hub.cpp",
                                           audio::SoundSet::glass,
                                           Color::rgb(0x80edd4),
                                           techniques};
        return info;
    }
    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        if (!context_.settings.reduced_motion)
            clock_ += dt;
        nav_.style.reduced_motion = items_.style.reduced_motion = dialog_.style.reduced_motion =
            keys_.style.reduced_motion = context_.settings.reduced_motion;
        if (service_.poll(snapshot_))
        {
            refresh();
        }
        if (dialog_.is_open())
        {
            if (dialog_.handle(input, feedback) == ui::Event::activated && dialog_.choice() == 1)
            {
                if (pending_download_)
                    service_.download(selected_);
            }
        }
        else if (edit_step_ >= 0)
        {
            keys_.set_length(static_cast<int>(edit_.size()));
            const auto event = keys_.handle(input, feedback);
            if (event == ui::Event::cancelled)
            {
                edit_step_ = -1;
                edit_.clear();
            }
            if (event == ui::Event::activated)
                accept_field(feedback);
        }
        else if (input.is_pressed(Action::west))
        {
            if (snapshot_.busy)
                service_.cancel();
            else
                service_.load();
        }
        else if (input.is_pressed(Action::north) && (section_ == 1 || section_ == 2) &&
                 !snapshot_.busy)
        {
            draft_ = {};
            edit_step_ = 0;
            edit_.clear();
            keys_.style.max_length = 120;
            keys_.enter();
            feedback.play(audio::Cue::select);
        }
        else if (input.is_pressed(Action::back) || input.nav == Direction::left)
        {
            sidebar_ = true;
            feedback.play(audio::Cue::back);
        }
        else if (sidebar_)
        {
            const auto event = nav_.handle(input, feedback);
            if (nav_.focus() != section_)
            {
                section_ = nav_.focus();
                refresh();
            }
            if (event == ui::Event::activated || input.nav == Direction::right)
            {
                sidebar_ = false;
                feedback.play(audio::Cue::select);
            }
        }
        else if (items_.handle(input, feedback) == ui::Event::activated)
            activate(feedback);
        nav_.set_active(sidebar_ && edit_step_ < 0 && !dialog_.is_open());
        items_.set_active(!sidebar_ && edit_step_ < 0 && !dialog_.is_open());
        nav_.update(dt);
        items_.update(dt);
        dialog_.update(dt);
        keys_.update(dt);
    }
    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::aurora;
        frame.backdrop.colors[0] = Color::rgb(0x070d18);
        frame.backdrop.colors[1] = Color::rgb(0x112339);
        frame.backdrop.colors[2] = Color::rgb(0x173c49);
        frame.backdrop.colors[3] = Color::rgb(0x29224b);
        frame.backdrop.time = clock_ * 0.2f;
        auto &list = frame.scene;
        const auto &fonts = context_.fonts;
        list.rounded_rect({96, 76, 60, 60}, 18, theme_.primary);
        ui::text(list, fonts.semibold, "H", 126, 119, 40, theme_.on_primary, gfx::Align::center);
        ui::text(list, fonts.semibold, "PS5 Hub", 176, 119, 38, theme_.text);
        ui::text(list, fonts.regular, "ÉDITION NATIVE  /  0.1 ALPHA", 1824, 113, 20,
                 theme_.text_muted, gfx::Align::right);
        list.rounded_rect({472, 230, 1352, 170}, 24, Color::rgb(0x172c3f, 0.85f));
        ui::text(list, fonts.display, sections[section_], 504, 301, 54, theme_.text);
        ui::text(list, fonts.regular, subtitles[section_], 506, 354, 25, theme_.text_muted);
        ui::text(list, fonts.semibold, "VOTRE ESPACE", 110, 235, 18, theme_.text_muted);
        ui::Canvas canvas{list, fonts, 0, clock_};
        nav_.draw(canvas);
        if (edit_step_ >= 0)
        {
            const char *labels[] = {"1 / 3 — Nom du contenu", "2 / 3 — Lien HTTPS direct",
                                    "3 / 3 — SHA-256 de l’auteur (optionnel)"};
            ui::text(list, fonts.semibold, labels[edit_step_], 504, 430, 28, theme_.text);
            keys_.draw(canvas);
            const auto shown = fonts.regular.font->fit(edit_, 25, 1256);
            ui::text(list, fonts.regular, shown.empty() ? "Saisissez votre texte…" : shown, 520,
                     912, 25, theme_.primary);
            ui::text(list, fonts.regular, edit_error_, 520, 950, 22, Color::rgb(0xf4c787));
        }
        else
        {
            items_.draw(canvas);
            const auto message = fonts.regular.font->fit(snapshot_.message, 23, 1280);
            ui::text(list, fonts.regular, message, 504, 900, 23, theme_.primary);
            if (snapshot_.busy)
                ui::text(list, fonts.mono, std::to_string(service_.bytes() / 1024) + " Kio reçus",
                         504, 940, 22, theme_.text_muted);
        }
        ui::text(list, fonts.regular, "13.60", 110, 902, 26, theme_.text);
        ui::text(list, fonts.regular, "Non testé sur console", 110, 940, 22, Color::rgb(0xf4c787));
        const ui::Hint hints[] = {
            {ui::Button::cross, edit_step_ >= 0 ? "Saisir" : "Ouvrir"},
            {ui::Button::circle, "Retour"},
            {ui::Button::triangle, edit_step_ >= 0 ? "Espace" : "Ajouter un lien"},
            {ui::Button::square, edit_step_ >= 0  ? "Effacer"
                                 : snapshot_.busy ? "Annuler"
                                                  : "Actualiser"}};
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints,
                       (edit_step_ >= 0 || section_ == 1 || section_ == 2) ? 4 : 2, 1824, true);
        frame.glass = dialog_.visible();
        ui::Canvas overlay{frame.overlay, fonts, frame.glass_texture, clock_};
        dialog_.draw(overlay);
    }
    std::span<const app::TourStep> tour() const override
    {
        static const app::TourStep steps[] = {
            {1.0f, 0, Direction::none, "accueil"},
            {0.4f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "homebrew"},
            {0.2f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.2f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.8f, action_bit(Action::back), Direction::none, "confirmation"},
            {0.3f, action_bit(Action::back), Direction::none, nullptr},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, action_bit(Action::north), Direction::none, "applications"},
            {0.7f, action_bit(Action::back), Direction::none, "ajout"},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "fichiers"},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "parametres"}};
        return steps;
    }

  private:
    void notice(std::string title, std::string body, app::Feedback &feedback)
    {
        pending_download_ = false;
        dialog_.open({ui::StatusKind::none,
                      std::move(title),
                      std::move(body),
                      {{"Fermer", ui::ButtonKind::secondary, false}},
                      0},
                     feedback);
    }
    void refresh()
    {
        items_.style.row_height = section_ == 4 ? 88.0f : 104.0f;
        std::vector<ui::ListItem> rows;
        indices_.clear();
        if (section_ == 0)
        {
            rows.push_back(
                row("Bienvenue chez vous", "Un hub C++ / OpenGL, conçu pour la manette.", "NATIF"));
            rows.push_back(row("Construisez votre bibliothèque",
                               "Ajoutez vos liens dans Homebrew ou Applications.",
                               std::to_string(snapshot_.entries.size())));
            rows.push_back(row("Installation système",
                               "Connecteur HyperHEN non validé : installation désactivée.",
                               "À VALIDER"));
        }
        else if (section_ == 1 || section_ == 2)
        {
            for (std::size_t i = 0; i < snapshot_.entries.size(); ++i)
            {
                const auto &e = snapshot_.entries[i];
                if ((section_ == 1 && e.kind != "pkg") || (section_ == 2 && e.kind == "pkg"))
                {
                    rows.push_back(row(e.title, e.description,
                                       e.kind == "link"  ? "PROJET"
                                       : e.kind == "pkg" ? "PKG"
                                                         : "ARCHIVE"));
                    indices_.push_back(i);
                }
            }
            if (rows.empty())
                rows.push_back(row("Votre bibliothèque commence ici",
                                   "Triangle : ajoutez un nom, une adresse et une empreinte."));
        }
        else if (section_ == 3)
        {
            for (const auto &name : snapshot_.files)
                rows.push_back(row(name, "Stockage personnel / downloads", "LOCAL"));
            if (rows.empty())
                rows.push_back(
                    row("Aucun téléchargement", "Les fichiers vérifiés apparaîtront ici."));
        }
        else
        {
            rows.push_back(row("Réduire les animations", "Préférence enregistrée automatiquement",
                               context_.settings.reduced_motion ? "OUI" : "NON"));
            rows.push_back(row("Effets sonores", "Activer ou couper les sons de navigation",
                               context_.settings.ui_volume ? "OUI" : "NON"));
            rows.push_back(row("Vérifier les mises à jour",
                               "Recherche la version publiée de PS5 Hub", "HTTPS"));
            rows.push_back(row("À propos et compatibilité",
                               "Versions HyperHEN / ShadowMountPlus inconnues", "0.1 ALPHA"));
        }
        const int focus = items_.focus();
        items_.set_items(std::move(rows));
        items_.set_focus(focus);
    }
    void activate(app::Feedback &feedback)
    {
        if (section_ == 4)
        {
            switch (items_.focus())
            {
            case 0:
                context_.settings.reduced_motion = !context_.settings.reduced_motion;
                context_.settings_changed = true;
                refresh();
                break;
            case 1:
                context_.settings.ui_volume = context_.settings.ui_volume ? 0 : 7;
                context_.settings_changed = true;
                refresh();
                break;
            case 2:
                if (!service_.check_update())
                    notice("Opération en cours",
                           "Attendez la fin du travail actuel ou annulez-le avec Carré.", feedback);
                break;
            default:
                notice("PS5 Hub — version alpha",
                       "GPL-3.0-or-later. Base BlackBearReloaded. Firmware 13.60, HyperHEN, "
                       "DualSense et ShadowMountPlus : validation matérielle à effectuer.",
                       feedback);
                break;
            }
        }
        else if ((section_ == 1 || section_ == 2) && !indices_.empty())
        {
            selected_ = snapshot_.entries[indices_[static_cast<std::size_t>(items_.focus())]];
            if (selected_.kind == "link")
                notice(selected_.title,
                       selected_.url + "\nFiche de référence. Ajoutez un lien direct vers une "
                                       "version publiée pour télécharger.",
                       feedback);
            else if (snapshot_.busy)
                notice("Opération en cours", "Attendez ou annulez avec Carré.", feedback);
            else
            {
                pending_download_ = true;
                dialog_.open({ui::StatusKind::none,
                              "Télécharger ce fichier ?",
                              selected_.title + "\n" + selected_.url +
                                  "\nSHA-256 requis. Limite : 2 Gio. Le téléchargement n’installe "
                                  "pas le paquet.",
                              {{"Annuler", ui::ButtonKind::secondary, false},
                               {"Télécharger", ui::ButtonKind::primary, false}},
                              0},
                             feedback);
            }
        }
        else if (section_ == 3)
            notice(
                "Fichiers téléchargés",
                "Consultation des téléchargements du hub. Installation système indisponible : "
                "aucun connecteur validé pour votre configuration. Consultez le guide USB / FTP.",
                feedback);
        else
            notice("Première version native",
                   "Navigation, catalogue personnel, fichiers, HTTPS et préférences. "
                   "L’installation des PKG et le lancement d’autres titres nécessitent encore une "
                   "intégration matérielle validée.",
                   feedback);
    }
    void accept_field(app::Feedback &feedback)
    {
        edit_error_.clear();
        if (edit_step_ == 0)
        {
            if (edit_.empty())
            {
                edit_error_ = "Indiquez un nom pour ce contenu.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.title = edit_;
            edit_.clear();
            edit_step_ = 1;
            keys_.style.max_length = 2048;
        }
        else if (edit_step_ == 1)
        {
            if (!hub::valid_url(edit_))
            {
                edit_error_ = "Saisissez une adresse commençant par https://, sans espace.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.url = edit_;
            edit_.clear();
            edit_step_ = 2;
            keys_.style.max_length = 64;
        }
        else
        {
            if (!edit_.empty() && !hub::valid_hash(edit_))
            {
                edit_error_ = "Le SHA-256 doit contenir 64 caractères : 0 à 9 et a à f.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.sha256 = edit_;
            draft_.kind = section_ == 1 ? "homebrew" : "pkg";
            draft_.version = "00.000.000";
            draft_.description = "Lien personnel — compatibilité à vérifier";
            draft_.id = "perso-" + std::to_string(static_cast<long long>(std::time(nullptr))) +
                        "-" + std::to_string(snapshot_.entries.size());
            service_.add(draft_);
            edit_step_ = -1;
            edit_.clear();
            feedback.play(audio::Cue::complete);
        }
        keys_.set_length(0);
    }
    app::Context &context_;
    hub::Service service_;
    hub::Snapshot snapshot_;
    ui::Theme theme_;
    ui::ListView nav_, items_;
    ui::Dialog dialog_;
    ui::Keyboard keys_;
    std::vector<std::size_t> indices_;
    hub::Entry selected_, draft_;
    std::string edit_, edit_error_;
    int section_ = 0, edit_step_ = -1;
    bool sidebar_ = true, pending_download_ = false;
    float clock_ = 0;
};
} // namespace
std::unique_ptr<app::Concept> make_hub(app::Context &c)
{
    return std::make_unique<Hub>(c);
}
} // namespace hui::concepts
