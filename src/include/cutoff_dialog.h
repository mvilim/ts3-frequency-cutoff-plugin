/*
 * Copyright (c) 2018 Michael Vilim
 *
 * This file is part of the teamspeak frequency cutoff filter plugin. It is
 * currently hosted at https://github.com/mvilim/ts3-frequency-cutoff-plugin
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

#include <freq_cutoff.h>
#include <ts3_log.h>

constexpr int MULTIPLIER = 100;
constexpr int MIN_CUTOFF = 0;
constexpr int MAX_CUTOFF = 10000 / MULTIPLIER;
constexpr int DEFAULT_CUTOFF = 4000 / MULTIPLIER;
constexpr int STEP_INCREMENT = 100 / MULTIPLIER;
constexpr int PAGE_INCREMENT = 1000 / MULTIPLIER;

class ConfigureCutoffDialog : public QDialog {
    const string uname;
    ApplicationFilterGroup& app_filter_group;
    QLabel* value_label;
    QSlider* slider;
    QCheckBox* enabled;
    map<const string, FilterConf> original_confs;

   public:
    ConfigureCutoffDialog(const string dname, const string uname,
                          ApplicationFilterGroup& app_filter_group,
                          QWidget* parent = NULL)
        : QDialog(parent), uname(uname), app_filter_group(app_filter_group) {
        this->setWindowTitle(("Frequency cutoff for " + dname).c_str());
        this->setAttribute(Qt::WA_DeleteOnClose);
        QGridLayout* layout = new QGridLayout(this);
        enabled = new QCheckBox("Frequency cutoff enabled");
        layout->addWidget(enabled, 0, 0, 1, 9);
        value_label = new QLabel();
        layout->addWidget(value_label, 1, 7, 1, 2);
        slider = new QSlider(Qt::Orientation::Horizontal);
        slider->setSingleStep(STEP_INCREMENT);
        slider->setPageStep(PAGE_INCREMENT);

        slider->setMinimum(MIN_CUTOFF);
        slider->setMaximum(MAX_CUTOFF);

        layout->addWidget(slider, 1, 0, 1, 7);

        QPushButton* cancel = new QPushButton("Cancel");
        layout->addWidget(cancel, 2, 0, 1, 3);
        QPushButton* remove = new QPushButton("Remove");
        layout->addWidget(remove, 2, 3, 1, 3);
        QPushButton* apply = new QPushButton("Apply");
        layout->addWidget(apply, 2, 6, 1, 3);

        QObject::connect(slider, &QSlider::valueChanged, this,
                         &ConfigureCutoffDialog::value_changed);
        QObject::connect(cancel, &QPushButton::released, this,
                         &ConfigureCutoffDialog::cancel);
        QObject::connect(apply, &QPushButton::released, this,
                         &ConfigureCutoffDialog::apply);
        QObject::connect(remove, &QPushButton::released, this,
                         &ConfigureCutoffDialog::remove);

        original_confs = *app_filter_group.load_atomic();
        if (original_confs.count(uname) > 0) {
            FilterConf& conf = original_confs.at(uname);
            enabled->setChecked(conf.enabled);
            slider->setValue(conf.coefficients.cutoff_freq / MULTIPLIER);
        } else {
            enabled->setChecked(false);
            slider->setValue(DEFAULT_CUTOFF);
        }

        update_label();

        QObject::connect(slider, &QSlider::sliderReleased, this,
                         &ConfigureCutoffDialog::apply_temporary);
        QObject::connect(enabled, &QCheckBox::stateChanged, this,
                         &ConfigureCutoffDialog::apply_temporary);
    }

   public slots:
    int slider_cutoff_value() { return slider->value() * MULTIPLIER; }

    void update_label() {
        value_label->setText(
            (std::to_string(slider_cutoff_value()) + " Hz").c_str());
    }

    void apply_current_state() {
        map<const string, FilterConf> updated_confs =
            *app_filter_group.load_atomic();
        FilterConf new_conf(enabled->isChecked(), slider_cutoff_value());
        if (updated_confs.count(uname) > 0) {
            updated_confs.at(uname) = new_conf;
        } else {
            updated_confs.emplace(uname, new_conf);
        }

        app_filter_group.store_atomic(updated_confs);
    }

    void apply_temporary() { apply_current_state(); }

    void value_changed() {
        update_label();
    }

    void cancel() {
        app_filter_group.store_atomic(original_confs);
        app_filter_group.persist();
        this->close();
    }

    void remove() {
        original_confs.erase(uname);
        app_filter_group.store_atomic(original_confs);
        app_filter_group.persist();
        this->close();
    }

    void apply() {
        apply_current_state();
        app_filter_group.persist();
        this->close();
    }
};
