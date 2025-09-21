use crate::args::Args;
use crate::dataset::Dataset;
use crate::utils::*;
use eframe::{egui, App, Frame};
use rand::Rng;
use egui_plot::{Legend, Line, Plot, PlotPoints};

#[derive(Debug, Clone)]
#[derive(PartialEq)]
pub enum FontSize {
   
    Small,
    Medium,
    Large,
    ExtraLarge,
}

impl FontSize {
    pub fn to_scale(&self) -> f32 {
        match self {
            FontSize::Small => 0.8,
            FontSize::Medium => 1.0,
            FontSize::Large => 1.3,
            FontSize::ExtraLarge => 1.6,
        }
    }
    
    pub fn to_string(&self) -> &'static str {
        match self {
            FontSize::Small => "Small",
            FontSize::Medium => "Medium",
            FontSize::Large => "Large", 
            FontSize::ExtraLarge => "Extra Large",
        }
    }
}

pub struct PlotterApp {
    pub datasets: Vec<Dataset>,
    pub show_grid: bool,
    pub show_legend: bool,
    pub next_name_index: usize,
    pub error_message: Option<String>,
    pub dark_mode: bool,
    pub screenshot_requested: bool,
    // Axis control fields
    pub use_custom_bounds: bool,
    pub custom_x_min: String,
    pub custom_x_max: String,
    pub custom_y_min: String,
    pub custom_y_max: String,
    pub x_padding_percent: f64,
    pub y_padding_percent: f64,
    pub show_axis_controls: bool,
    // Custom ticks
    pub custom_x_ticks: String,
    pub custom_y_ticks: String,
    pub use_custom_x_ticks: bool,
    pub use_custom_y_ticks: bool,
    // Data manipulation fields
    pub show_data_manipulation: bool,
    pub rolling_window_size: usize,
    pub selected_dataset_for_processing: usize,
    // Color management
    pub show_color_picker: bool,
    pub selected_dataset_for_color: usize,
    // Font and legend controls
    pub tick_font_size: FontSize,
    pub legend_title: String,
    pub show_legend_controls: bool,
}

impl Default for PlotterApp {
    fn default() -> Self {
        Self {
            datasets: Vec::new(),
            show_grid: false,
            show_legend: true,
            next_name_index: 1,
            error_message: None,
            dark_mode: true,
            screenshot_requested: false,
            use_custom_bounds: false,
            custom_x_min: String::new(),
            custom_x_max: String::new(),
            custom_y_min: String::new(),
            custom_y_max: String::new(),
            x_padding_percent: 5.0,
            y_padding_percent: 5.0,
            show_axis_controls: false,
            custom_x_ticks: String::new(),
            custom_y_ticks: String::new(),
            use_custom_x_ticks: false,
            use_custom_y_ticks: false,
            show_data_manipulation: false,
            rolling_window_size: 10,
            selected_dataset_for_processing: 0,
            show_color_picker: false,
            selected_dataset_for_color: 0,
            tick_font_size: FontSize::Medium,
            legend_title: "Datasets".to_string(),
            show_legend_controls: false,
        }
    }
}

impl App for PlotterApp {
    fn update(&mut self, ctx: &egui::Context, frame: &mut Frame) {
        if self.dark_mode {
            ctx.set_visuals(egui::Visuals::dark())
        } else {
            ctx.set_visuals(egui::Visuals::light());
        }

        // Main application window
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            ui.horizontal(|ui| {
                if ui.button("Open File").clicked() {
                    if let Some(path) = pick_file() {
                        match path.extension().and_then(|ext| ext.to_str()) {
                            Some("csv") => match load_csv_points(&path) {
                                Ok(points) => {
                                    let name = format!("data{}", self.next_name_index);
                                    self.next_name_index += 1;
                                    let color = get_default_color((self.datasets.len()) % 8);
                                    self.datasets.push(Dataset { name, points, color });
                                    self.error_message = None;
                                }
                                Err(e) => {
                                    self.error_message = Some(format!("Failed to load CSV: {}", e));
                                }
                            },
                            Some("xvg") => match load_xvg_points(&path) {
                                Ok(points) => {
                                    let name = format!("data{}", self.next_name_index);
                                    self.next_name_index += 1;
                                    let color = get_default_color((self.datasets.len()) % 8);
                                    self.datasets.push(Dataset { name, points, color });
                                    self.error_message = None;
                                }
                                Err(e) => {
                                    self.error_message = Some(format!("Failed to load XVG: {}", e));
                                }
                            },
                            _ => {
                                self.error_message = Some(
                                    "Unsupported file type. Please select a CSV or XVG file."
                                        .to_string(),
                                );
                            }
                        }
                    }
                }

                if ui.button("Export Plot as PNG").clicked() {
                    let axis_config = if self.use_custom_bounds {
                        Some(AxisConfig {
                            x_min: self.custom_x_min.parse().ok(),
                            x_max: self.custom_x_max.parse().ok(),
                            y_min: self.custom_y_min.parse().ok(),
                            y_max: self.custom_y_max.parse().ok(),
                            x_padding_percent: self.x_padding_percent / 100.0,
                            y_padding_percent: self.y_padding_percent / 100.0,
                            custom_x_ticks: if self.use_custom_x_ticks { Some(parse_custom_ticks(&self.custom_x_ticks)) } else { None },
                            custom_y_ticks: if self.use_custom_y_ticks { Some(parse_custom_ticks(&self.custom_y_ticks)) } else { None },
                        })
                    } else {
                        None
                    };

                    match export_plot_as_png_with_config(
                        &self.datasets,
                        self.dark_mode,
                        self.show_grid,
                        axis_config,
                        &self.tick_font_size) {
                        Ok(()) => {
                            self.error_message = Some("Plot exported successfully!".to_string())
                        }
                        Err(e) => {
                            self.error_message = Some(format!("Failed to export plot: {}", e))
                        }
                    }
                }

                if ui.button("Clear datasets").clicked() {
                    self.datasets.clear();
                }

                ui.separator();
                ui.checkbox(&mut self.show_grid, "Grid");
                ui.checkbox(&mut self.show_legend, "Legend");

                // Toggle for axis controls window
                if ui.button("⚙ Axis Controls").clicked() {
                    self.show_axis_controls = !self.show_axis_controls;
                }

                // Toggle for data manipulation window
                if ui.button("📊 Data Processing").clicked() {
                    self.show_data_manipulation = !self.show_data_manipulation;
                }

                // Toggle for color picker window
                if ui.button("🎨 Colors").clicked() {
                    self.show_color_picker = !self.show_color_picker;
                }

                // Toggle for legend controls window
                if ui.button("📝 Legend & Fonts").clicked() {
                    self.show_legend_controls = !self.show_legend_controls;
                }

                ui.horizontal(|ui| {
                    ui.label("Dark Mode:");
                    let switch_size = egui::vec2(40.0, 20.0);
                    let (rect, response) =
                        ui.allocate_exact_size(switch_size, egui::Sense::click());
                    if response.clicked() {
                        self.dark_mode = !self.dark_mode;
                    }

                    let bg_color = if self.dark_mode {
                        egui::Color32::from_rgb(0, 120, 215)
                    } else {
                        egui::Color32::from_rgb(200, 200, 200)
                    };

                    ui.painter()
                        .rect_filled(rect, switch_size.y * 0.5, bg_color);

                    let handle_radius = switch_size.y * 0.4;
                    let handle_center = if self.dark_mode {
                        egui::pos2(rect.max.x - handle_radius * 1.2, rect.center().y)
                    } else {
                        egui::pos2(rect.min.x + handle_radius * 1.2, rect.center().y)
                    };

                    ui.painter()
                        .circle_filled(handle_center, handle_radius, egui::Color32::WHITE);
                });

                ui.separator();
                if ui.button("Add random").clicked() {
                    let mut rng = rand::rng();
                    let mut pts = Vec::new();
                    let n = 120usize;
                    for i in 0..n {
                        let x = i as f64 / n as f64 * 10.0;
                        let y = rng.random_range(-2.0..2.0);
                        pts.push([x, y]);
                    }
                    let name = format!("random{}", self.next_name_index);
                    self.next_name_index += 1;
                    let color = get_default_color(self.datasets.len() % 8);
                    self.datasets.push(Dataset { name, points: pts, color });
                }
            });

            if let Some(ref error) = self.error_message {
                ui.colored_label(egui::Color32::RED, error);
            }
        });

        // Separate axis controls window
        if self.show_axis_controls {
            egui::Window::new("Axis Controls")
                .resizable(true)
                .default_width(400.0)
                .default_height(300.0)
                .show(ctx, |ui| {
                    ui.checkbox(&mut self.use_custom_bounds, "Override Automatic Axis Ranges");

                    if self.use_custom_bounds {
                        ui.separator();

                        // Auto-fill button
                        if ui.button("Auto-fill from data").clicked() {
                            if let Some((min_x, max_x, min_y, max_y)) = get_data_bounds(&self.datasets) {
                                self.custom_x_min = min_x.to_string();
                                self.custom_x_max = max_x.to_string();
                                self.custom_y_min = format!("{:.3}", min_y);
                                self.custom_y_max = format!("{:.3}", max_y);
                            }
                        }

                        ui.separator();

                        // X-axis controls
                        ui.group(|ui| {
                            ui.label("X-Axis Range");
                            ui.horizontal(|ui| {
                                ui.label("Min:");
                                ui.text_edit_singleline(&mut self.custom_x_min);
                                ui.label("Max:");
                                ui.text_edit_singleline(&mut self.custom_x_max);
                            });
                            ui.horizontal(|ui| {
                                ui.label("Padding:");
                                ui.add(egui::Slider::new(&mut self.x_padding_percent, 0.0..=20.0)
                                       .suffix("%"));
                            });
                            
                            ui.checkbox(&mut self.use_custom_x_ticks, "Custom X-axis ticks");
                            if self.use_custom_x_ticks {
                                ui.label("X-axis tick values (comma-separated):");
                                ui.text_edit_multiline(&mut self.custom_x_ticks);
                                ui.small("Example: 0, 250, 500");
                            }
                        });

                        ui.separator();

                        // Y-axis controls
                        ui.group(|ui| {
                            ui.label("Y-Axis Range");
                            ui.horizontal(|ui| {
                                ui.label("Min:");
                                ui.text_edit_singleline(&mut self.custom_y_min);
                                ui.label("Max:");
                                ui.text_edit_singleline(&mut self.custom_y_max);
                            });
                            ui.horizontal(|ui| {
                                ui.label("Padding:");
                                ui.add(egui::Slider::new(&mut self.y_padding_percent, 0.0..=20.0)
                                       .suffix("%"));
                            });
                            
                            ui.checkbox(&mut self.use_custom_y_ticks, "Custom Y-axis ticks");
                            if self.use_custom_y_ticks {
                                ui.label("Y-axis tick values (comma-separated):");
                                ui.text_edit_multiline(&mut self.custom_y_ticks);
                                ui.small("Example: 0.0, 0.5, 1.0");
                            }
                        });
                    }
                });
        }

        // Legend and Font controls window
        if self.show_legend_controls {
            egui::Window::new("Legend & Font Controls")
                .resizable(true)
                .default_width(350.0)
                .default_height(400.0)
                .show(ctx, |ui| {
                    ui.heading("Font Settings");
                    ui.separator();
                    
                    ui.horizontal(|ui| {
                        ui.label("Tick font size:");
                        egui::ComboBox::from_label("")
                            .selected_text(self.tick_font_size.to_string())
                            .show_ui(ui, |ui| {
                                ui.selectable_value(&mut self.tick_font_size, FontSize::Small, "Small");
                                ui.selectable_value(&mut self.tick_font_size, FontSize::Medium, "Medium");
                                ui.selectable_value(&mut self.tick_font_size, FontSize::Large, "Large");
                                ui.selectable_value(&mut self.tick_font_size, FontSize::ExtraLarge, "Extra Large");
                            });
                    });

                    ui.add_space(15.0);
                    ui.heading("Legend Settings");
                    ui.separator();

                    ui.horizontal(|ui| {
                        ui.label("Legend title:");
                        ui.text_edit_singleline(&mut self.legend_title);
                    });

                    ui.add_space(10.0);
                    
                    if !self.datasets.is_empty() {
                        ui.label("Dataset labels:");
                        ui.separator();
                        
                        for (i, dataset) in self.datasets.iter_mut().enumerate() {
                            ui.horizontal(|ui| {
                                // Color indicator
                                let color = egui::Color32::from_rgb(dataset.color[0], dataset.color[1], dataset.color[2]);
                                ui.add(egui::Button::new("").fill(color).min_size(egui::vec2(15.0, 15.0)));
                                
                                ui.label(format!("{}:", i + 1));
                                ui.text_edit_singleline(&mut dataset.name);
                            });
                        }
                    } else {
                        ui.label("No datasets loaded. Load data to edit legend labels.");
                    }
                });
        }

        // Separate data manipulation window
        if self.show_data_manipulation {
            egui::Window::new("Data Processing")
                .resizable(true)
                .default_width(350.0)
                .default_height(250.0)
                .show(ctx, |ui| {
                    if self.datasets.is_empty() {
                        ui.label("No datasets loaded. Load data first to enable processing.");
                        return;
                    }

                    ui.heading("Rolling Average");
                    ui.separator();

                    // Dataset selection
                    ui.horizontal(|ui| {
                        ui.label("Dataset:");
                        egui::ComboBox::from_label("")
                            .selected_text(&self.datasets[self.selected_dataset_for_processing].name)
                            .show_ui(ui, |ui| {
                                for (i, dataset) in self.datasets.iter().enumerate() {
                                    ui.selectable_value(&mut self.selected_dataset_for_processing, i, &dataset.name);
                                }
                            });
                    });

                    ui.add_space(10.0);

                    // Window size setting
                    ui.horizontal(|ui| {
                        ui.label("Window size:");
                        ui.add(egui::Slider::new(&mut self.rolling_window_size, 2..=100).text("points"));
                    });

                    ui.add_space(10.0);

                    // Show preview info
                    if self.selected_dataset_for_processing < self.datasets.len() {
                        let dataset = &self.datasets[self.selected_dataset_for_processing];
                        ui.label(format!("Original dataset: {} points", dataset.points.len()));
                        
                        if dataset.points.len() >= self.rolling_window_size {
                            let result_points = dataset.points.len() - self.rolling_window_size + 1;
                            ui.label(format!("Rolling average will have: {} points", result_points));
                        } else {
                            ui.colored_label(egui::Color32::from_rgb(255, 165, 0), 
                                "Warning: Window size larger than dataset!");
                        }
                    }

                    ui.add_space(15.0);

                    // Compute button
                    if ui.button("🔄 Compute Rolling Average").clicked() {
                        if self.selected_dataset_for_processing < self.datasets.len() {
                            let source_dataset = &self.datasets[self.selected_dataset_for_processing];
                            
                            if source_dataset.points.len() >= self.rolling_window_size {
                                match compute_rolling_average(&source_dataset.points, self.rolling_window_size) {
                                    Ok(rolling_avg_points) => {
                                        let new_name = format!("{}_rolling_avg_{}", 
                                            source_dataset.name, self.rolling_window_size);
                                        let new_dataset = Dataset {
                                            name: new_name,
                                            points: rolling_avg_points,
                                            color: get_default_color(self.datasets.len() % 8),
                                        };
                                        self.datasets.push(new_dataset);
                                        self.error_message = Some(format!(
                                            "Rolling average computed! Added as new dataset."
                                        ));
                                    },
                                    Err(e) => {
                                        self.error_message = Some(format!("Error computing rolling average: {}", e));
                                    }
                                }
                            } else {
                                self.error_message = Some(
                                    "Window size must be smaller than or equal to dataset size.".to_string()
                                );
                            }
                        }
                    }

                    ui.add_space(10.0);
                    ui.separator();
                    ui.small("The rolling average will be added as a new dataset that you can export or analyze separately.");
                });
        }
 

        // Separate color picker window
        if self.show_color_picker {
            egui::Window::new("Dataset Colors")
                .resizable(true)
                .default_width(300.0)
                .default_height(400.0)
                .show(ctx, |ui| {
                    if self.datasets.is_empty() {
                        ui.label("No datasets loaded. Load data first to customize colors.");
                        return;
                    }

                    ui.heading("Dataset Colors");
                    ui.separator();

                    for (i, dataset) in self.datasets.iter_mut().enumerate() {
                        ui.horizontal(|ui| {
                            // Color square button
                            let color_button_size = egui::vec2(30.0, 20.0);
                            let color = egui::Color32::from_rgb(dataset.color[0], dataset.color[1], dataset.color[2]);
                            
                            if ui
                                .add(egui::Button::new("").fill(color).min_size(color_button_size))
                                .clicked()
                            {
                                self.selected_dataset_for_color = i;
                            }

                            ui.label(&dataset.name);
                        });

                        // Color picker for selected dataset
                        if i == self.selected_dataset_for_color {
                            ui.indent("color_picker", |ui| {
                                let mut color = egui::Color32::from_rgb(dataset.color[0], dataset.color[1], dataset.color[2]);
                                ui.color_edit_button_srgba(&mut color);
                                dataset.color = [color.r(), color.g(), color.b()];
                            });
                        }

                        ui.add_space(5.0);
                    }

                    ui.separator();
                    
                    if ui.button("Reset to Default Colors").clicked() {
                        for (i, dataset) in self.datasets.iter_mut().enumerate() {
                            dataset.color = get_default_color(i % 8);
                        }
                    }
                });
        }

        // Main plot area with applied axis settings
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Plot area – pan with mouse, zoom with scroll");
            ui.add_space(6.0);

            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.set_width(200.0);
                    ui.label("Datasets:");

                    let mut remove_index: Option<usize> = None;
                    for (i, ds) in self.datasets.iter_mut().enumerate() {
                        ui.horizontal(|ui| {
                            // Clickable color square
                            let color_size = egui::vec2(15.0, 15.0);
                            let color = egui::Color32::from_rgb(ds.color[0], ds.color[1], ds.color[2]);
                            
                            if ui
                                .add(egui::Button::new("").fill(color).min_size(color_size))
                                .on_hover_text("Click to change color")
                                .clicked()
                            {
                                self.selected_dataset_for_color = i;
                                self.show_color_picker = true;
                            }

                            ui.label(format!("{}:", i + 1));
                            ui.label(&ds.name);
                            if ui.small_button("x").clicked() {
                                remove_index = Some(i);
                            }
                        });
                    }
                    if let Some(i) = remove_index {
                        self.datasets.remove(i);
                    }
                });

                ui.separator();

                ui.vertical(|ui| {
                    let mut plot = Plot::new("main_plot")
                        .view_aspect(2.0)
                        .height(500.0)
                        .width(1000.0)
                        .show_axes([true, true])
                        .show_grid([self.show_grid, self.show_grid]);

                    // Apply custom bounds to GUI plot in real-time
                    if self.use_custom_bounds {
                        if let (Ok(min_x), Ok(max_x)) = (self.custom_x_min.parse::<f64>(), self.custom_x_max.parse::<f64>()) {
                            if let (Ok(min_y), Ok(max_y)) = (self.custom_y_min.parse::<f64>(), self.custom_y_max.parse::<f64>()) {
                                // Apply padding to GUI plot as well
                                let x_range = max_x - min_x;
                                let y_range = max_y - min_y;
                                let x_padding = x_range * (self.x_padding_percent / 100.0);
                                let y_padding = y_range * (self.y_padding_percent / 100.0);
                                
                                let padded_min_x = min_x - x_padding;
                                let padded_max_x = max_x + x_padding;
                                let padded_min_y = min_y - y_padding;
                                let padded_max_y = max_y + y_padding;
                                
                                plot = plot.include_x(padded_min_x).include_x(padded_max_x)
                                          .include_y(padded_min_y).include_y(padded_max_y);
                            }
                        }
                    }

                    if self.show_legend {
                        // Create legend with custom title
                        let mut legend = Legend::default();
                        legend = legend.text_style(egui::TextStyle::Body);
                        plot = plot.legend(legend);
                    }

                    plot.show(ui, |plot_ui| {
                        // Display legend title if legend is shown
                        if self.show_legend && !self.legend_title.is_empty() {
                            // Note: egui_plot doesn't directly support legend titles,
                            // so we'd need to draw this manually or use a workaround
                        }
                        
                        for ds in &self.datasets {
                            let color = egui::Color32::from_rgb(ds.color[0], ds.color[1], ds.color[2]);
                            let line = Line::new(PlotPoints::new(ds.points.clone()))
                                .name(&ds.name)
                                .color(color);
                            plot_ui.line(line);
                        }
                    });
                });
            });
        });
    }
}
