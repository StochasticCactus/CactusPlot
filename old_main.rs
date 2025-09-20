use clap::Parser;
use eframe::{egui, App, Frame};
use egui_plot::{Legend, Line, Plot, PlotPoints};
use rand::prelude::*;
use rand::thread_rng;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "CactusPlot")]
#[command(about = "A simple but elegant plotting application")]
struct Args {
    files: Vec<String>,

    #[arg(long)]
    grid: bool,

    #[arg(long)]
    no_legend: bool,
}

struct Dataset {
    name: String,
    points: Vec<[f64; 2]>,
    color: [u8; 3], // RGB color for this dataset
}

struct PlotterApp {
    datasets: Vec<Dataset>,
    show_grid: bool,
    show_legend: bool,
    next_name_index: usize,
    error_message: Option<String>,
    dark_mode: bool,
    screenshot_requested: bool,
    // Axis control fields
    use_custom_bounds: bool,
    custom_x_min: String,
    custom_x_max: String,
    custom_y_min: String,
    custom_y_max: String,
    x_padding_percent: f64,
    y_padding_percent: f64,
    show_axis_controls: bool,
    // Custom ticks
    custom_x_ticks: String,  // Comma-separated values
    custom_y_ticks: String,  // Comma-separated values
    use_custom_x_ticks: bool,
    use_custom_y_ticks: bool,
    // Data manipulation fields
    show_data_manipulation: bool,
    rolling_window_size: usize,
    selected_dataset_for_processing: usize,
    // Color management
    show_color_picker: bool,
    selected_dataset_for_color: usize,
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
                        axis_config) {
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
            ui.heading("Plot area — pan with mouse, zoom with scroll");
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
                        plot = plot.legend(Legend::default());
                    }

                    plot.show(ui, |plot_ui| {
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

#[derive(Debug, Clone)]
struct AxisConfig {
    x_min: Option<f64>,
    x_max: Option<f64>,
    y_min: Option<f64>,
    y_max: Option<f64>,
    x_padding_percent: f64,
    y_padding_percent: f64,
    custom_x_ticks: Option<Vec<f64>>,
    custom_y_ticks: Option<Vec<f64>>,
}

// Helper function to parse custom ticks from comma-separated string
fn parse_custom_ticks(ticks_str: &str) -> Vec<f64> {
    ticks_str
        .split(',')
        .filter_map(|s| s.trim().parse::<f64>().ok())
        .collect()
}

// Helper function to compute rolling average
fn compute_rolling_average(points: &[[f64; 2]], window_size: usize) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    if window_size == 0 {
        return Err("Window size must be greater than 0".into());
    }
    
    if points.len() < window_size {
        return Err("Window size cannot be larger than dataset size".into());
    }
    
    let mut result = Vec::new();
    
    // Compute rolling average
    for i in 0..=(points.len() - window_size) {
        let window_slice = &points[i..i + window_size];
        
        // Calculate average X and Y for this window
        let avg_x: f64 = window_slice.iter().map(|p| p[0]).sum::<f64>() / window_size as f64;
        let avg_y: f64 = window_slice.iter().map(|p| p[1]).sum::<f64>() / window_size as f64;
        
        result.push([avg_x, avg_y]);
    }
    
    Ok(result)
}

// Helper function to get data bounds
fn get_data_bounds(datasets: &[Dataset]) -> Option<(f64, f64, f64, f64)> {
    if datasets.is_empty() {
        return None;
    }
    
    let mut min_x = f64::INFINITY;
    let mut max_x = f64::NEG_INFINITY;
    let mut min_y = f64::INFINITY;
    let mut max_y = f64::NEG_INFINITY;
    
    for dataset in datasets {
        for point in &dataset.points {
            min_x = min_x.min(point[0]);
            max_x = max_x.max(point[0]);
            min_y = min_y.min(point[1]);
            max_y = max_y.max(point[1]);
        }
    }
    
    Some((min_x, max_x, min_y, max_y))
}

// Modified export function that accepts axis configuration
fn export_plot_as_png_with_config(
    datasets: &[Dataset],
    dark_mode: bool,
    show_grid: bool,
    axis_config: Option<AxisConfig>,
) -> Result<(), Box<dyn std::error::Error>> {
    if datasets.is_empty() {
        return Err("No data to export".into());
    }

    if let Some(path) = rfd::FileDialog::new()
        .add_filter("PNG Image", &["png"])
        .set_file_name("plot.png")
        .save_file()
    {
        let width = 1200u32;
        let height = 800u32;

        // Calculate bounds based on configuration
        let (min_x, max_x, min_y, max_y) = if let Some(ref config) = axis_config {
            calculate_custom_bounds(datasets, config)?
        } else {
            calculate_auto_bounds(datasets)
        };

        println!("Using axis bounds:");
        println!("  X: {} to {}", min_x, max_x);
        println!("  Y: {} to {}", min_y, max_y);

        let (bg_color, grid_color, axis_color, text_color) = if dark_mode {
            (
                image::Rgb([27, 27, 27]),
                image::Rgb([60, 60, 60]),
                image::Rgb([180, 180, 180]),
                image::Rgb([255, 255, 255]),
            )
        } else {
            (
                image::Rgb([248, 248, 248]),
                image::Rgb([200, 200, 200]),
                image::Rgb([100, 100, 100]),
                image::Rgb([0, 0, 0]),
            )
        };

        let mut img_buffer = image::RgbImage::new(width, height);
        for pixel in img_buffer.pixels_mut() {
            *pixel = bg_color;
        }

        let margin_left = 80u32;
        let margin_right = 40u32;
        let margin_top = 40u32;
        let margin_bottom = 60u32;
        let plot_width = width - margin_left - margin_right;
        let plot_height = height - margin_top - margin_bottom;

        // Draw grid if requested
        if show_grid {
            let num_v_lines = 8;
            for i in 1..num_v_lines {
                let x = margin_left + (i * plot_width / num_v_lines);
                for y in margin_top..(height - margin_bottom) {
                    if y % 3 == 0 {
                        img_buffer.put_pixel(x, y, grid_color);
                    }
                }
            }
            let num_h_lines = 6;
            for i in 1..num_h_lines {
                let y = margin_top + (i * plot_height / num_h_lines);
                for x in margin_left..(width - margin_right) {
                    if x % 3 == 0 {
                        img_buffer.put_pixel(x, y, grid_color);
                    }
                }
            }
        }

        // Draw axes
        let x_axis_y = height - margin_bottom;
        let y_axis_x = margin_left;
        for x in margin_left..(width - margin_right) {
            img_buffer.put_pixel(x, x_axis_y, axis_color);
        }
        for y in margin_top..(height - margin_bottom) {
            img_buffer.put_pixel(y_axis_x, y, axis_color);
        }

        // Draw axis labels with custom ticks if specified
        draw_axis_labels_with_custom_ticks(
            &mut img_buffer,
            min_x,
            max_x,
            min_y,
            max_y,
            margin_left,
            margin_bottom,
            plot_width,
            plot_height,
            width,
            height,
            text_color,
            axis_config.as_ref(),
        );

        // Draw datasets using their custom colors
        for (dataset_idx, dataset) in datasets.iter().enumerate() {
            if dataset.points.is_empty() {
                continue;
            }
            
            // Use the dataset's custom color instead of the predefined palette
            let rgb_color = image::Rgb(dataset.color);
            
            for window in dataset.points.windows(2) {
                let p1 = &window[0];
                let p2 = &window[1];
                let x1 =
                    margin_left + ((p1[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
                let y1 = height
                    - margin_bottom
                    - ((p1[1] - min_y) / (max_y - min_y) * plot_height as f64) as u32;
                let x2 =
                    margin_left + ((p2[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
                let y2 = height
                    - margin_bottom
                    - ((p2[1] - min_y) / (max_y - min_y) * plot_height as f64) as u32;
                draw_thick_line(&mut img_buffer, x1, y1, x2, y2, rgb_color, 2);
            }
        }

        img_buffer.save(&path)?;
        println!("Plot exported as: {}", path.display());
    }
    Ok(())
}

fn calculate_custom_bounds(datasets: &[Dataset], config: &AxisConfig) -> Result<(f64, f64, f64, f64), Box<dyn std::error::Error>> {
    let (data_min_x, data_max_x, data_min_y, data_max_y) = get_data_bounds(datasets)
        .ok_or("No data available")?;

    let base_min_x = config.x_min.unwrap_or(data_min_x);
    let base_max_x = config.x_max.unwrap_or(data_max_x);
    let base_min_y = config.y_min.unwrap_or(data_min_y);
    let base_max_y = config.y_max.unwrap_or(data_max_y);

    let x_range = base_max_x - base_min_x;
    let y_range = base_max_y - base_min_y;

    let x_padding = x_range * config.x_padding_percent;
    let y_padding = y_range * config.y_padding_percent;

    let min_x = base_min_x - x_padding;
    let max_x = base_max_x + x_padding;
    let min_y = base_min_y - y_padding;
    let max_y = base_max_y + y_padding;

    Ok((min_x, max_x, min_y, max_y))
}

fn calculate_auto_bounds(datasets: &[Dataset]) -> (f64, f64, f64, f64) {
    let (mut min_x, mut max_x, mut min_y, mut max_y) = get_data_bounds(datasets)
        .unwrap_or((0.0, 1.0, 0.0, 1.0));

    if (max_x - min_x).abs() < f64::EPSILON {
        let center = min_x;
        min_x = center - 1.0;
        max_x = center + 1.0;
    }

    if (max_y - min_y).abs() < f64::EPSILON {
        let center = min_y;
        min_y = center - 1.0;
        max_y = center + 1.0;
    }

    let x_range = max_x - min_x;
    let y_range = max_y - min_y;
    let padding_percent = 0.05;

    let x_padding = x_range * padding_percent;
    let y_padding = y_range * padding_percent;

    let padded_min_x = min_x - x_padding;
    let padded_min_y = if min_y > 0.0 {
        (min_y - y_padding).max(0.0)
    } else {
        min_y - y_padding
    };

    (padded_min_x, max_x + x_padding, padded_min_y, max_y + y_padding)
}

// Enhanced axis label drawing with custom ticks support
fn draw_axis_labels_with_custom_ticks(
    img: &mut image::RgbImage,
    min_x: f64,
    max_x: f64,
    min_y: f64,
    max_y: f64,
    margin_left: u32,
    margin_bottom: u32,
    plot_width: u32,
    plot_height: u32,
    width: u32,
    height: u32,
    color: image::Rgb<u8>,
    axis_config: Option<&AxisConfig>,
) {
    // X-axis ticks and labels
    let x_tick_values: Vec<f64> = if let Some(config) = axis_config {
        if let Some(ref custom_x_ticks) = config.custom_x_ticks {
            // Use custom ticks, but filter to only those within range
            custom_x_ticks.iter()
                .filter(|&&tick| tick >= min_x && tick <= max_x)
                .copied()
                .collect()
        } else {
            // Use default 6 evenly spaced ticks
            (0..=6).map(|i| min_x + (max_x - min_x) * (i as f64 / 6.0)).collect()
        }
    } else {
        // Use default 6 evenly spaced ticks
        (0..=6).map(|i| min_x + (max_x - min_x) * (i as f64 / 6.0)).collect()
    };

    for &tick_value in &x_tick_values {
        let x_pos = margin_left + ((tick_value - min_x) / (max_x - min_x) * plot_width as f64) as u32;
        let tick_y = height - margin_bottom;
        
        // Draw tick mark
        for dy in 0..8 {
            if tick_y + dy < height {
                img.put_pixel(x_pos, tick_y + dy, color);
            }
        }
        
        // Draw label
        let text = format_number(tick_value);
        let text_width = text.len() as u32 * 6;
        let label_x = if x_pos >= text_width / 2 {
            x_pos - text_width / 2
        } else {
            0
        };
        
        draw_number_pixels(img, label_x, tick_y + 20, tick_value, color);
    }

    // Y-axis ticks and labels
    let y_tick_values: Vec<f64> = if let Some(config) = axis_config {
        if let Some(ref custom_y_ticks) = config.custom_y_ticks {
            // Use custom ticks, but filter to only those within range
            custom_y_ticks.iter()
                .filter(|&&tick| tick >= min_y && tick <= max_y)
                .copied()
                .collect()
        } else {
            // Use default 6 evenly spaced ticks
            (0..=6).map(|i| min_y + (max_y - min_y) * (i as f64 / 6.0)).collect()
        }
    } else {
        // Use default 6 evenly spaced ticks
        (0..=6).map(|i| min_y + (max_y - min_y) * (i as f64 / 6.0)).collect()
    };

    for &tick_value in &y_tick_values {
        let y_pos = height - margin_bottom - ((tick_value - min_y) / (max_y - min_y) * plot_height as f64) as u32;
        let tick_x = margin_left;
        
        // Draw tick mark
        for dx in 0..8 {
            if tick_x >= dx {
                img.put_pixel(tick_x - dx, y_pos, color);
            }
        }
        
        // Draw label
        let text = format_number(tick_value);
        let text_width = text.len() as u32 * 6;
        let label_x = if tick_x >= text_width + 15 {
            tick_x - text_width - 15
        } else {
            0
        };
        
        draw_number_pixels(img, label_x, y_pos.saturating_sub(3), tick_value, color);
    }
}

// Keep all your existing drawing functions unchanged
fn draw_number_pixels(
    img: &mut image::RgbImage,
    x: u32,
    y: u32,
    value: f64,
    color: image::Rgb<u8>,
) {
    let text = format_number(value);
    for (i, ch) in text.chars().enumerate() {
        let char_x = x + (i as u32 * 6);
        draw_char_pixels(img, char_x, y, ch, color);
    }
}

fn draw_char_pixels(img: &mut image::RgbImage, x: u32, y: u32, ch: char, color: image::Rgb<u8>) {
    let pattern = match ch {
        '0' => [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
        '1' => [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
        '2' => [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],
        '3' => [0b11111, 0b00010, 0b00100, 0b00110, 0b00001, 0b10001, 0b01110],
        '4' => [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],
        '5' => [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
        '6' => [0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
        '7' => [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
        '8' => [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
        '9' => [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100],
        '.' => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100],
        '-' => [0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000],
        'K' => [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
        'M' => [0b10001, 0b11011, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001],
        'e' => [0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b10001, 0b01110],
        _ => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000],
    };

    for (row, &pattern_row) in pattern.iter().enumerate() {
        for col in 0..5 {
            if (pattern_row >> (4 - col)) & 1 == 1 {
                let px = x + col;
                let py = y + row as u32;
                if px < img.width() && py < img.height() {
                    img.put_pixel(px, py, color);
                }
            }
        }
    }
}

fn draw_thick_line(
    img: &mut image::RgbImage,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color: image::Rgb<u8>,
    thickness: u32,
) {
    for offset in 0..thickness {
        let offset = offset as i32 - (thickness as i32 / 2);
        draw_line_offset(img, x0, y0, x1, y1, color, offset, 0);
        if offset != 0 {
            draw_line_offset(img, x0, y0, x1, y1, color, 0, offset);
        }
    }
}

fn draw_line_offset(
    img: &mut image::RgbImage,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color: image::Rgb<u8>,
    offset_x: i32,
    offset_y: i32,
) {
    let dx = (x1 as i32 - x0 as i32).abs();
    let dy = (y1 as i32 - y0 as i32).abs();
    let sx = if x0 < x1 { 1 } else { -1 };
    let sy = if y0 < y1 { 1 } else { -1 };
    let mut err = dx - dy;
    let mut x = x0 as i32;
    let mut y = y0 as i32;

    loop {
        let px = x + offset_x;
        let py = y + offset_y;

        if px >= 0 && py >= 0 && (px as u32) < img.width() && (py as u32) < img.height() {
            img.put_pixel(px as u32, py as u32, color);
        }

        if x == x1 as i32 && y == y1 as i32 {
            break;
        }

        let e2 = 2 * err;
        if e2 > -dy {
            err -= dy;
            x += sx;
        }
        if e2 < dx {
            err += dx;
            y += sy;
        }
    }
}

fn load_csv_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    let mut rdr = csv::Reader::from_path(path)?;
    let mut out = Vec::new();
    for result in rdr.records() {
        let record = result?;
        if record.len() < 2 {
            continue;
        }
        if let (Ok(x), Ok(y)) = (
            record.get(0).unwrap().trim().parse::<f64>(),
            record.get(1).unwrap().trim().parse::<f64>(),
        ) {
            out.push([x, y]);
        }
    }
    Ok(out)
}

fn load_xvg_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut points = Vec::new();

    for line_result in reader.lines() {
        let line = line_result?;
        let line = line.trim();

        if line.is_empty() || line.starts_with('#') || line.starts_with('@') {
            continue;
        }

        let parts: Vec<&str> = line.split_whitespace().collect();

        if parts.len() < 2 {
            continue;
        }

        if let (Ok(x), Ok(y)) = (parts[0].parse::<f64>(), parts[1].parse::<f64>()) {
            points.push([x, y]);
        }
    }

    Ok(points)
}

fn format_number(value: f64) -> String {
    let abs_value = value.abs();
    
    if abs_value >= 1_000_000.0 {
        let m_value = value / 1_000_000.0;
        format!("{:.1}M", m_value)
    } else if abs_value >= 100_000.0 {
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 10_000.0 {
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 1000.0 {
        format!("{:.0}", value)
    } else if abs_value >= 1.0 {
        if value.fract().abs() < 0.01 {
            format!("{:.0}", value)
        } else {
            format!("{:.1}", value)
        }
    } else if abs_value >= 0.01 {
        format!("{:.3}", value)
    } else if abs_value > f64::EPSILON {
        format!("{:.4}", value)
    } else {
        "0".to_string()
    }
}

// Get default color palette
fn get_default_color(index: usize) -> [u8; 3] {
    let colors = [
        [31, 120, 180],   // Blue
        [255, 127, 14],   // Orange  
        [44, 160, 44],    // Green
        [214, 39, 40],    // Red
        [148, 103, 189],  // Purple
        [140, 86, 75],    // Brown
        [227, 119, 194],  // Pink
        [127, 127, 127],  // Gray
    ];
    colors[index % colors.len()]
}

fn pick_file() -> Option<PathBuf> {
    rfd::FileDialog::new()
        .add_filter("csv", &["csv"])
        .add_filter("xvg", &["xvg"])
        .pick_file()
}

fn main() {
    let args = Args::parse();
    let mut options = eframe::NativeOptions::default();
    options.default_theme = eframe::Theme::Light;

    eframe::run_native(
        "CactusPlot",
        options,
        Box::new(move |_cc| {
            let mut app = PlotterApp::default();
            app.show_legend = !args.no_legend;

            for file in args.files {
                if let Ok(points) = load_csv_points(&PathBuf::from(&file)) {
                    let color = get_default_color(app.datasets.len() % 8);
                    app.datasets.push(Dataset {
                        name: file.clone(),
                        points,
                        color,
                    });
                    app.next_name_index += 1;
                }
            }

            Box::new(app)
        }),
    )
    .unwrap();
}
