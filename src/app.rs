use crate::data_editor::DataEditor;
use crate::dataset::Dataset;
use crate::utils::*;
use eframe::{egui, App, Frame};
use egui_plot::{Legend, Line, Plot, PlotPoints};
use rand::Rng;

#[derive(PartialEq, Debug, Clone)]
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

#[derive(Debug, Clone)]
pub struct SubplotConfig {
    pub show_grid: bool,
    pub show_legend: bool,
    pub legend_title: String,
    pub use_custom_bounds: bool,
    pub custom_x_min: String,
    pub custom_x_max: String,
    pub custom_y_min: String,
    pub custom_y_max: String,
    pub x_padding_percent: f64,
    pub y_padding_percent: f64,
    pub custom_x_ticks: String,
    pub custom_y_ticks: String,
    pub use_custom_x_ticks: bool,
    pub use_custom_y_ticks: bool,
    pub title: String,
}

impl Default for SubplotConfig {
    fn default() -> Self {
        Self {
            show_grid: false,
            show_legend: true,
            legend_title: "Datasets".to_string(),
            use_custom_bounds: false,
            custom_x_min: String::new(),
            custom_x_max: String::new(),
            custom_y_min: String::new(),
            custom_y_max: String::new(),
            x_padding_percent: 5.0,
            y_padding_percent: 5.0,
            custom_x_ticks: String::new(),
            custom_y_ticks: String::new(),
            use_custom_x_ticks: false,
            use_custom_y_ticks: false,
            title: String::new(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Subplot {
    pub id: String,
    pub datasets: Vec<Dataset>,
    pub config: SubplotConfig,
}

impl Subplot {
    pub fn new(id: String) -> Self {
        Self {
            id,
            datasets: Vec::new(),
            config: SubplotConfig::default(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SubplotLayout {
    Single,      // 1x1
    Horizontal2, // 1x2
    Vertical2,   // 2x1
    Grid2x2,     // 2x2
    Grid3x1,     // 3x1
    Grid1x3,     // 1x3
    Grid3x2,     // 3x2
    Grid2x3,     // 2x3
}

impl SubplotLayout {
    pub fn to_string(&self) -> &'static str {
        match self {
            SubplotLayout::Single => "Single (1x1)",
            SubplotLayout::Horizontal2 => "Horizontal (1x2)",
            SubplotLayout::Vertical2 => "Vertical (2x1)",
            SubplotLayout::Grid2x2 => "Grid (2x2)",
            SubplotLayout::Grid3x1 => "Grid (3x1)",
            SubplotLayout::Grid1x3 => "Grid (1x3)",
            SubplotLayout::Grid3x2 => "Grid (3x2)",
            SubplotLayout::Grid2x3 => "Grid (2x3)",
        }
    }

    pub fn dimensions(&self) -> (usize, usize) {
        match self {
            SubplotLayout::Single => (1, 1),
            SubplotLayout::Horizontal2 => (1, 2),
            SubplotLayout::Vertical2 => (2, 1),
            SubplotLayout::Grid2x2 => (2, 2),
            SubplotLayout::Grid3x1 => (3, 1),
            SubplotLayout::Grid1x3 => (1, 3),
            SubplotLayout::Grid3x2 => (3, 2),
            SubplotLayout::Grid2x3 => (2, 3),
        }
    }

    pub fn subplot_count(&self) -> usize {
        let (rows, cols) = self.dimensions();
        rows * cols
    }
}

pub struct PlotterApp {
    // Subplot system
    pub subplots: Vec<Subplot>,
    pub subplot_layout: SubplotLayout,
    pub active_subplot: usize,
    pub show_subplot_controls: bool,

    // Global settings
    pub next_name_index: usize,
    pub error_message: Option<String>,
    pub dark_mode: bool,
    pub screenshot_requested: bool,
    pub tick_font_size: FontSize,

    // UI state
    pub show_axis_controls: bool,
    pub show_data_manipulation: bool,
    pub show_color_picker: bool,
    pub show_legend_controls: bool,

    // Data manipulation fields
    pub rolling_window_size: usize,
    pub selected_dataset_for_processing: usize,
    pub selected_dataset_for_color: usize,
    pub data_editor: DataEditor,
}

impl Default for PlotterApp {
    fn default() -> Self {
        let mut app = Self {
            subplots: Vec::new(),
            subplot_layout: SubplotLayout::Single,
            active_subplot: 0,
            show_subplot_controls: false,
            next_name_index: 1,
            error_message: None,
            dark_mode: true,
            screenshot_requested: false,
            tick_font_size: FontSize::Medium,
            show_axis_controls: false,
            show_data_manipulation: false,
            show_color_picker: false,
            show_legend_controls: false,
            rolling_window_size: 10,
            selected_dataset_for_processing: 0,
            selected_dataset_for_color: 0,
            data_editor: DataEditor::default(),
        };

        // Initialize with one subplot
        app.ensure_subplots_match_layout();
        app
    }
}

impl PlotterApp {
    fn ensure_subplots_match_layout(&mut self) {
        let required_count = self.subplot_layout.subplot_count();

        // Remove excess subplots
        if self.subplots.len() > required_count {
            self.subplots.truncate(required_count);
        }

        // Add missing subplots
        while self.subplots.len() < required_count {
            let id = format!("subplot_{}", self.subplots.len());
            self.subplots.push(Subplot::new(id));
        }

        // Ensure active subplot is valid
        if self.active_subplot >= self.subplots.len() {
            self.active_subplot = 0;
        }
    }

    pub fn get_active_subplot_mut(&mut self) -> Option<&mut Subplot> {
        self.subplots.get_mut(self.active_subplot)
    }

    pub fn get_active_subplot(&self) -> Option<&Subplot> {
        self.subplots.get(self.active_subplot)
    }
}

impl App for PlotterApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut Frame) {
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
                                    let color = get_default_color(
                                        self.get_active_subplot().map_or(0, |s| s.datasets.len())
                                            % 8,
                                    );
                                    if let Some(subplot) = self.get_active_subplot_mut() {
                                        subplot.datasets.push(Dataset {
                                            name,
                                            points,
                                            color,
                                        });
                                    }
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
                                    let color = get_default_color(
                                        self.get_active_subplot().map_or(0, |s| s.datasets.len())
                                            % 8,
                                    );
                                    if let Some(subplot) = self.get_active_subplot_mut() {
                                        subplot.datasets.push(Dataset {
                                            name,
                                            points,
                                            color,
                                        });
                                    }
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
                    match export_subplots_as_png(
                        &self.subplots,
                        &self.subplot_layout,
                        self.dark_mode,
                        &self.tick_font_size,
                    ) {
                        Ok(()) => {
                            self.error_message = Some("Plot exported successfully!".to_string())
                        }
                        Err(e) => {
                            self.error_message = Some(format!("Failed to export plot: {}", e))
                        }
                    }
                }

                if ui.button("Clear Active Subplot").clicked() {
                    if let Some(subplot) = self.get_active_subplot_mut() {
                        subplot.datasets.clear();
                    }
                }

                if ui.button("Clear All Subplots").clicked() {
                    for subplot in &mut self.subplots {
                        subplot.datasets.clear();
                    }
                }

                ui.separator();

                // Global controls
                if let Some(subplot) = self.get_active_subplot_mut() {
                    ui.checkbox(&mut subplot.config.show_grid, "Grid");
                    ui.checkbox(&mut subplot.config.show_legend, "Legend");
                }

                // Toggle for subplot layout controls
                if ui.button("🔲 Subplots").clicked() {
                    self.show_subplot_controls = !self.show_subplot_controls;
                }

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

                if ui.button("📊 Data Editor").clicked() {
                    self.data_editor.show_editor = !self.data_editor.show_editor;
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
                    let color = get_default_color(
                        self.get_active_subplot().map_or(0, |s| s.datasets.len()) % 8,
                    );
                    if let Some(subplot) = self.get_active_subplot_mut() {
                        subplot.datasets.push(Dataset {
                            name,
                            points: pts,
                            color,
                        });
                    }
                }
            });

            if let Some(ref error) = self.error_message {
                ui.colored_label(egui::Color32::RED, error);
            }
        });

        // Subplot controls window
        if self.show_subplot_controls {
            egui::Window::new("Subplot Layout")
                .resizable(true)
                .default_width(350.0)
                .default_height(400.0)
                .show(ctx, |ui| {
                    ui.heading("Layout Configuration");
                    ui.separator();

                    ui.horizontal(|ui| {
                        ui.label("Layout:");
                        egui::ComboBox::from_label("")
                            .selected_text(self.subplot_layout.to_string())
                            .show_ui(ui, |ui| {
                                for layout in &[
                                    SubplotLayout::Single,
                                    SubplotLayout::Horizontal2,
                                    SubplotLayout::Vertical2,
                                    SubplotLayout::Grid2x2,
                                    SubplotLayout::Grid3x1,
                                    SubplotLayout::Grid1x3,
                                    SubplotLayout::Grid3x2,
                                    SubplotLayout::Grid2x3,
                                ] {
                                    if ui.selectable_value(&mut self.subplot_layout, *layout, layout.to_string()).clicked() {
                                        self.ensure_subplots_match_layout();
                                    }
                                }
                            });
                    });

                    ui.add_space(10.0);
                    ui.separator();
                    ui.heading("Active Subplot");

                    ui.horizontal(|ui| {
                        ui.label("Active:");
                        for (i, _) in self.subplots.iter().enumerate() {
                            if ui.selectable_label(self.active_subplot == i, format!("{}", i + 1)).clicked() {
                                self.active_subplot = i;
                            }
                        }
                    });

                    ui.add_space(10.0);

                    if let Some(subplot) = self.get_active_subplot() {
                        ui.label(format!("Active subplot: {} (ID: {})", self.active_subplot + 1, subplot.id));
                        ui.label(format!("Datasets: {}", subplot.datasets.len()));
                    }

                    ui.add_space(10.0);
                    ui.separator(); 
                    // Subplot titles
                    ui.heading("Subplot Titles");
                    for (i, subplot) in self.subplots.iter_mut().enumerate() {
                        ui.horizontal(|ui| {
                            ui.label(format!("Subplot {}:", i + 1));
                            ui.text_edit_singleline(&mut subplot.config.title);
                        });
                    }

                    ui.add_space(10.0);
                    ui.separator();
                    ui.small("Changes to layout will reorganize your data. Active subplot receives new data and operations.");
                });
        }

        // Data editor window
        if self.data_editor.show_editor {
            if let Some(subplot) = self.get_active_subplot() {
                let mut datasets = subplot.datasets.clone();
                self.data_editor.show_data_editor_window(ctx, &mut datasets);

                if let Some(subplot_mut) = self.get_active_subplot_mut() {
                    subplot_mut.datasets = datasets;
                }
            }
        }

        // Show other control windows
        self.show_control_windows(ctx);

        // Main plot area with subplots
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Multi-plot area – pan with mouse, zoom with scroll");
            ui.add_space(6.0);

            let (rows, cols) = self.subplot_layout.dimensions();

            // Create subplot grid
            egui::Grid::new("subplot_grid")
                .num_columns(cols)
                .spacing([10.0, 10.0])
                .show(ui, |ui| {
                    for row in 0..rows {
                        for col in 0..cols {
                            let subplot_index = row * cols + col;
                            if subplot_index < self.subplots.len() {
                                let is_active = subplot_index == self.active_subplot;
                                self.render_subplot(ui, subplot_index, is_active);
                            }
                        }
                        ui.end_row();
                    }
                });
        });
    }
}

impl PlotterApp {
    fn render_subplot(&mut self, ui: &mut egui::Ui, subplot_index: usize, is_active: bool) {
        // Get subplot data first to avoid borrowing conflicts
        let subplot_title = if let Some(subplot) = self.subplots.get(subplot_index) {
            if !subplot.config.title.is_empty() {
                format!("Subplot {}: {}", subplot_index + 1, subplot.config.title)
            } else {
                format!("Subplot {}", subplot_index + 1)
            }
        } else {
            return;
        };

        let subplot_datasets: Vec<(String, [u8; 3])> =
            if let Some(subplot) = self.subplots.get(subplot_index) {
                subplot
                    .datasets
                    .iter()
                    .map(|ds| (ds.name.clone(), ds.color))
                    .collect()
            } else {
                Vec::new()
            };

        ui.vertical(|ui| {
            // Subplot header with selection
            ui.horizontal(|ui| {
                if ui.selectable_label(is_active, &subplot_title).clicked() {
                    self.active_subplot = subplot_index;
                }

                if is_active {
                    ui.colored_label(egui::Color32::from_rgb(0, 200, 0), " (Active)");
                }
            });

            // Dataset list for this subplot
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.set_width(150.0);
                    ui.label("Datasets:");

                    let mut remove_index: Option<usize> = None;
                    for (i, (name, color)) in subplot_datasets.iter().enumerate() {
                        ui.horizontal(|ui| {
                            // Clickable color square
                            let color_size = egui::vec2(12.0, 12.0);
                            let egui_color = egui::Color32::from_rgb(color[0], color[1], color[2]);

                            if ui
                                .add(egui::Button::new("").fill(egui_color).min_size(color_size))
                                .on_hover_text("Click to change color")
                                .clicked()
                            {
                                self.selected_dataset_for_color = i;
                                self.active_subplot = subplot_index;
                                self.show_color_picker = true;
                            }

                            ui.label(name);
                            if ui.small_button("×").clicked() {
                                remove_index = Some(i);
                                self.active_subplot = subplot_index;
                            }
                        });
                    }

                    // Apply removal after iteration
                    if let Some(remove_idx) = remove_index {
                        if let Some(subplot_mut) = self.subplots.get_mut(subplot_index) {
                            subplot_mut.datasets.remove(remove_idx);
                        }
                    }
                });

                ui.separator();

                // Plot area
                ui.vertical(|ui| {
                    let plot_width = match self.subplot_layout {
                        SubplotLayout::Single => 800.0,
                        SubplotLayout::Horizontal2 | SubplotLayout::Vertical2 => 400.0,
                        SubplotLayout::Grid2x2 => 350.0,
                        SubplotLayout::Grid3x1 | SubplotLayout::Grid1x3 => 250.0,
                        SubplotLayout::Grid3x2 | SubplotLayout::Grid2x3 => 200.0,
                    };

                    let plot_height = match self.subplot_layout {
                        SubplotLayout::Single => 400.0,
                        SubplotLayout::Horizontal2 | SubplotLayout::Vertical2 => 300.0,
                        SubplotLayout::Grid2x2 => 250.0,
                        SubplotLayout::Grid3x1 | SubplotLayout::Grid1x3 => 200.0,
                        SubplotLayout::Grid3x2 | SubplotLayout::Grid2x3 => 150.0,
                    };

                    if let Some(subplot) = self.subplots.get(subplot_index) {
                        let mut plot = Plot::new(&format!("plot_{}", subplot_index))
                            .height(plot_height)
                            .width(plot_width)
                            .show_axes([true, true])
                            .show_grid([subplot.config.show_grid, subplot.config.show_grid]);

                        // Apply custom bounds if configured
                        if subplot.config.use_custom_bounds {
                            if let (Ok(min_x), Ok(max_x)) = (
                                subplot.config.custom_x_min.parse::<f64>(),
                                subplot.config.custom_x_max.parse::<f64>(),
                            ) {
                                if let (Ok(min_y), Ok(max_y)) = (
                                    subplot.config.custom_y_min.parse::<f64>(),
                                    subplot.config.custom_y_max.parse::<f64>(),
                                ) {
                                    let x_range = max_x - min_x;
                                    let y_range = max_y - min_y;
                                    let x_padding =
                                        x_range * (subplot.config.x_padding_percent / 100.0);
                                    let y_padding =
                                        y_range * (subplot.config.y_padding_percent / 100.0);

                                    plot = plot
                                        .include_x(min_x - x_padding)
                                        .include_x(max_x + x_padding)
                                        .include_y(min_y - y_padding)
                                        .include_y(max_y + y_padding);
                                }
                            }
                        } else {
                            // FIXED: Automatically include data bounds when custom bounds are not set
                            if !subplot.datasets.is_empty() {
                                if let Some((min_x, max_x, min_y, max_y)) =
                                    get_data_bounds(&subplot.datasets)
                                {
                                    // Add some padding (5% by default)
                                    let x_range = max_x - min_x;
                                    let y_range = max_y - min_y;

                                    // Handle case where range is zero (single point or constant values)
                                    let x_padding =
                                        if x_range > 0.0 { x_range * 0.05 } else { 1.0 };
                                    let y_padding =
                                        if y_range > 0.0 { y_range * 0.05 } else { 1.0 };

                                    plot = plot
                                        .include_x(min_x - x_padding)
                                        .include_x(max_x + x_padding)
                                        .include_y(min_y - y_padding)
                                        .include_y(max_y + y_padding);
                                }
                            }
                        }

                        if subplot.config.show_legend {
                            plot = plot.legend(Legend::default());
                        }

                        plot.show(ui, |plot_ui| {
                            for ds in &subplot.datasets {
                                let color =
                                    egui::Color32::from_rgb(ds.color[0], ds.color[1], ds.color[2]);
                                let line = Line::new(PlotPoints::new(ds.points.clone()))
                                    .name(&ds.name)
                                    .color(color);
                                plot_ui.line(line);
                            }
                        });
                    }
                });
            });
        });
    }
    fn show_control_windows(&mut self, ctx: &egui::Context) {
        // Axis controls window
        if self.show_axis_controls {
            egui::Window::new("Axis Controls")
                .resizable(true)
                .default_width(400.0)
                .default_height(300.0)
                .show(ctx, |ui| {
                    if let Some(subplot) = self.get_active_subplot_mut() {
                        ui.checkbox(
                            &mut subplot.config.use_custom_bounds,
                            "Override Automatic Axis Ranges",
                        );

                        if subplot.config.use_custom_bounds {
                            ui.separator();

                            // Auto-fill button
                            if ui.button("Auto-fill from data").clicked() {
                                if let Some((min_x, max_x, min_y, max_y)) =
                                    get_data_bounds(&subplot.datasets)
                                {
                                    subplot.config.custom_x_min = min_x.to_string();
                                    subplot.config.custom_x_max = max_x.to_string();
                                    subplot.config.custom_y_min = format!("{:.3}", min_y);
                                    subplot.config.custom_y_max = format!("{:.3}", max_y);
                                }
                            }

                            ui.separator();

                            // X-axis controls
                            ui.group(|ui| {
                                ui.label("X-Axis Range");
                                ui.horizontal(|ui| {
                                    ui.label("Min:");
                                    ui.text_edit_singleline(&mut subplot.config.custom_x_min);
                                    ui.label("Max:");
                                    ui.text_edit_singleline(&mut subplot.config.custom_x_max);
                                });
                                ui.horizontal(|ui| {
                                    ui.label("Padding:");
                                    ui.add(
                                        egui::Slider::new(
                                            &mut subplot.config.x_padding_percent,
                                            0.0..=20.0,
                                        )
                                        .suffix("%"),
                                    );
                                });

                                ui.checkbox(
                                    &mut subplot.config.use_custom_x_ticks,
                                    "Custom X-axis ticks",
                                );
                                if subplot.config.use_custom_x_ticks {
                                    ui.label("X-axis tick values (comma-separated):");
                                    ui.text_edit_multiline(&mut subplot.config.custom_x_ticks);
                                    ui.small("Example: 0, 250, 500");
                                }
                            });

                            ui.separator();

                            // Y-axis controls
                            ui.group(|ui| {
                                ui.label("Y-Axis Range");
                                ui.horizontal(|ui| {
                                    ui.label("Min:");
                                    ui.text_edit_singleline(&mut subplot.config.custom_y_min);
                                    ui.label("Max:");
                                    ui.text_edit_singleline(&mut subplot.config.custom_y_max);
                                });
                                ui.horizontal(|ui| {
                                    ui.label("Padding:");
                                    ui.add(
                                        egui::Slider::new(
                                            &mut subplot.config.y_padding_percent,
                                            0.0..=20.0,
                                        )
                                        .suffix("%"),
                                    );
                                });

                                ui.checkbox(
                                    &mut subplot.config.use_custom_y_ticks,
                                    "Custom Y-axis ticks",
                                );
                                if subplot.config.use_custom_y_ticks {
                                    ui.label("Y-axis tick values (comma-separated):");
                                    ui.text_edit_multiline(&mut subplot.config.custom_y_ticks);
                                    ui.small("Example: 0.0, 0.5, 1.0");
                                }
                            });
                        }
                    } else {
                        ui.label("No active subplot selected.");
                    }
                });
        }

        // Data manipulation window (similar to before, but operates on active subplot)
        if self.show_data_manipulation {
            self.show_data_manipulation_window(ctx);
        }

        // Color picker window (similar to before, but for active subplot)
        if self.show_color_picker {
            self.show_color_picker_window(ctx);
        }

        // Legend controls window
        if self.show_legend_controls {
            self.show_legend_controls_window(ctx);
        }
    }

    fn show_data_manipulation_window(&mut self, ctx: &egui::Context) {
        egui::Window::new("Data Processing")
            .resizable(true)
            .default_width(350.0)
            .default_height(250.0)
            .show(ctx, |ui| {
                let subplot_info = if let Some(subplot) = self.get_active_subplot() {
                    if subplot.datasets.is_empty() {
                        ui.label(
                            "No datasets in active subplot. Load data first to enable processing.",
                        );
                        return;
                    }
                    let dataset_names: Vec<String> =
                        subplot.datasets.iter().map(|d| d.name.clone()).collect();
                    Some((dataset_names, subplot.datasets.len()))
                } else {
                    ui.label("No active subplot selected.");
                    return;
                };

                let (dataset_names, dataset_count) = subplot_info.unwrap();

                ui.heading("Rolling Average");
                ui.separator();

                // Dataset selection
                ui.horizontal(|ui| {
                    ui.label("Dataset:");
                    if self.selected_dataset_for_processing < dataset_names.len() {
                        egui::ComboBox::from_label("")
                            .selected_text(&dataset_names[self.selected_dataset_for_processing])
                            .show_ui(ui, |ui| {
                                for (i, name) in dataset_names.iter().enumerate() {
                                    ui.selectable_value(
                                        &mut self.selected_dataset_for_processing,
                                        i,
                                        name,
                                    );
                                }
                            });
                    }
                });

                ui.add_space(10.0);

                // Window size setting
                ui.horizontal(|ui| {
                    ui.label("Window size:");
                    ui.add(
                        egui::Slider::new(&mut self.rolling_window_size, 2..=100).text("points"),
                    );
                });

                ui.add_space(10.0);

                // Show preview info
                if let Some(subplot) = self.get_active_subplot() {
                    if let Some(dataset) =
                        subplot.datasets.get(self.selected_dataset_for_processing)
                    {
                        ui.label(format!("Original dataset: {} points", dataset.points.len()));

                        if dataset.points.len() >= self.rolling_window_size {
                            let result_points = dataset.points.len() - self.rolling_window_size + 1;
                            ui.label(format!(
                                "Rolling average will have: {} points",
                                result_points
                            ));
                        } else {
                            ui.colored_label(
                                egui::Color32::from_rgb(255, 165, 0),
                                "Warning: Window size larger than dataset!",
                            );
                        }
                    }
                }

                ui.add_space(15.0);

                // Compute button
                if ui.button("🔄 Compute Rolling Average").clicked() {
                    if let Some(subplot) = self.get_active_subplot() {
                        if let Some(source_dataset) =
                            subplot.datasets.get(self.selected_dataset_for_processing)
                        {
                            if source_dataset.points.len() >= self.rolling_window_size {
                                match compute_rolling_average(
                                    &source_dataset.points,
                                    self.rolling_window_size,
                                ) {
                                    Ok(rolling_avg_points) => {
                                        let new_name = format!(
                                            "{}_rolling_avg_{}",
                                            source_dataset.name, self.rolling_window_size
                                        );
                                        let new_dataset = Dataset {
                                            name: new_name,
                                            points: rolling_avg_points,
                                            color: get_default_color(dataset_count % 8),
                                        };
                                        if let Some(subplot_mut) = self.get_active_subplot_mut() {
                                            subplot_mut.datasets.push(new_dataset);
                                        }
                                        self.error_message = Some(format!(
                                            "Rolling average computed! Added to active subplot."
                                        ));
                                    }
                                    Err(e) => {
                                        self.error_message =
                                            Some(format!("Error computing rolling average: {}", e));
                                    }
                                }
                            } else {
                                self.error_message = Some(
                                    "Window size must be smaller than or equal to dataset size."
                                        .to_string(),
                                );
                            }
                        }
                    }
                }

                ui.add_space(10.0);
                ui.separator();
                ui.small(
                    "The rolling average will be added as a new dataset in the active subplot.",
                );
            });
    }

    fn show_color_picker_window(&mut self, ctx: &egui::Context) {
        egui::Window::new("Dataset Colors")
            .resizable(true)
            .default_width(300.0)
            .default_height(400.0)
            .show(ctx, |ui| {
                // Get subplot info first to avoid borrowing conflicts
                let subplot_info = if let Some(subplot) = self.get_active_subplot() {
                    if subplot.datasets.is_empty() {
                        ui.label(
                            "No datasets in active subplot. Load data first to customize colors.",
                        );
                        return;
                    }
                    let dataset_info: Vec<(String, [u8; 3])> = subplot
                        .datasets
                        .iter()
                        .map(|ds| (ds.name.clone(), ds.color))
                        .collect();
                    Some(dataset_info)
                } else {
                    ui.label("No active subplot selected.");
                    return;
                };

                let dataset_info = subplot_info.unwrap();
                let mut selected_color_changed = None;
                let mut reset_colors = false;

                ui.heading("Dataset Colors (Active Subplot)");
                ui.separator();

                for (i, (name, color)) in dataset_info.iter().enumerate() {
                    ui.horizontal(|ui| {
                        // Color square button
                        let color_button_size = egui::vec2(30.0, 20.0);
                        let egui_color = egui::Color32::from_rgb(color[0], color[1], color[2]);

                        if ui
                            .add(
                                egui::Button::new("")
                                    .fill(egui_color)
                                    .min_size(color_button_size),
                            )
                            .clicked()
                        {
                            self.selected_dataset_for_color = i;
                        }

                        ui.label(name);
                    });

                    // Color picker for selected dataset
                    if i == self.selected_dataset_for_color {
                        ui.indent("color_picker", |ui| {
                            let mut egui_color =
                                egui::Color32::from_rgb(color[0], color[1], color[2]);
                            if ui.color_edit_button_srgba(&mut egui_color).changed() {
                                selected_color_changed =
                                    Some((i, [egui_color.r(), egui_color.g(), egui_color.b()]));
                            }
                        });
                    }

                    ui.add_space(5.0);
                }

                ui.separator();

                if ui.button("Reset to Default Colors").clicked() {
                    reset_colors = true;
                }

                // Apply changes after UI is done
                if let Some((index, new_color)) = selected_color_changed {
                    if let Some(subplot) = self.get_active_subplot_mut() {
                        if let Some(dataset) = subplot.datasets.get_mut(index) {
                            dataset.color = new_color;
                        }
                    }
                }

                if reset_colors {
                    if let Some(subplot) = self.get_active_subplot_mut() {
                        for (i, dataset) in subplot.datasets.iter_mut().enumerate() {
                            dataset.color = get_default_color(i % 8);
                        }
                    }
                }
            });
    }

    fn show_legend_controls_window(&mut self, ctx: &egui::Context) {
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
                            ui.selectable_value(
                                &mut self.tick_font_size,
                                FontSize::Medium,
                                "Medium",
                            );
                            ui.selectable_value(&mut self.tick_font_size, FontSize::Large, "Large");
                            ui.selectable_value(
                                &mut self.tick_font_size,
                                FontSize::ExtraLarge,
                                "Extra Large",
                            );
                        });
                });

                ui.add_space(15.0);
                ui.heading("Legend Settings (Active Subplot)");
                ui.separator();

                if let Some(subplot) = self.get_active_subplot_mut() {
                    ui.horizontal(|ui| {
                        ui.label("Legend title:");
                        ui.text_edit_singleline(&mut subplot.config.legend_title);
                    });

                    ui.add_space(10.0);

                    if !subplot.datasets.is_empty() {
                        ui.label("Dataset labels:");
                        ui.separator();

                        for (i, dataset) in subplot.datasets.iter_mut().enumerate() {
                            ui.horizontal(|ui| {
                                // Color indicator
                                let color = egui::Color32::from_rgb(
                                    dataset.color[0],
                                    dataset.color[1],
                                    dataset.color[2],
                                );
                                ui.add(
                                    egui::Button::new("")
                                        .fill(color)
                                        .min_size(egui::vec2(15.0, 15.0)),
                                );

                                ui.label(format!("{}:", i + 1));
                                ui.text_edit_singleline(&mut dataset.name);
                            });
                        }
                    } else {
                        ui.label("No datasets in active subplot. Load data to edit legend labels.");
                    }
                } else {
                    ui.label("No active subplot selected.");
                }
            });
    }
}
