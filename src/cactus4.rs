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
}


struct PlotterApp {
    datasets: Vec<Dataset>,
    show_grid: bool,
    show_legend: bool,
    next_name_index: usize,
    error_message: Option<String>,
    dark_mode: bool,
    screenshot_requested: bool,
    // New fields for axis control
    use_custom_bounds: bool,
    custom_x_min: String,
    custom_x_max: String,
    custom_y_min: String,
    custom_y_max: String,
    x_padding_percent: f64,
    y_padding_percent: f64,
    show_axis_controls: bool,
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
            // Default values for axis controls
            use_custom_bounds: false,
            custom_x_min: String::new(),
            custom_x_max: String::new(),
            custom_y_min: String::new(),
            custom_y_max: String::new(),
            x_padding_percent: 5.0, // 5% padding
            y_padding_percent: 5.0,
            show_axis_controls: false,
        }
    }
}

// Modified update function with axis controls UI
impl App for PlotterApp {
    fn update(&mut self, ctx: &egui::Context, frame: &mut Frame) {
        if self.dark_mode {
            ctx.set_visuals(egui::Visuals::dark())
        } else {
            ctx.set_visuals(egui::Visuals::light());
        }

        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            ui.horizontal(|ui| {
                if ui.button("Open File").clicked() {
                    if let Some(path) = pick_file() {
                        match path.extension().and_then(|ext| ext.to_str()) {
                            Some("csv") => match load_csv_points(&path) {
                                Ok(points) => {
                                    let name = format!("data{}", self.next_name_index);
                                    self.next_name_index += 1;
                                    self.datasets.push(Dataset { name, points });
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
                                    self.datasets.push(Dataset { name, points });
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

                // Toggle for axis controls
                if ui.button("⚙ Axis Controls").clicked() {
                    self.show_axis_controls = !self.show_axis_controls;
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
                    self.datasets.push(Dataset { name, points: pts });
                }
            });

            if let Some(ref error) = self.error_message {
                ui.colored_label(egui::Color32::RED, error);
            }

            // Axis controls panel
            if self.show_axis_controls {
                ui.separator();
                ui.horizontal(|ui| {
                    ui.checkbox(&mut self.use_custom_bounds, "Use Custom Axis Ranges");

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
                    }
                });

                if self.use_custom_bounds {
                    ui.horizontal(|ui| {
                        ui.label("X-axis:");
                        ui.label("Min:");
                        ui.text_edit_singleline(&mut self.custom_x_min);
                        ui.label("Max:");
                        ui.text_edit_singleline(&mut self.custom_x_max);

                        ui.separator();
                        ui.label("Padding:");
                        ui.add(egui::Slider::new(&mut self.x_padding_percent, 0.0..=20.0)
                               .suffix("%")
                               .text("X"));
                    });

                    ui.horizontal(|ui| {
                        ui.label("Y-axis:");
                        ui.label("Min:");
                        ui.text_edit_singleline(&mut self.custom_y_min);
                        ui.label("Max:");
                        ui.text_edit_singleline(&mut self.custom_y_max);

                        ui.separator();
                        ui.label("Padding:");
                        ui.add(egui::Slider::new(&mut self.y_padding_percent, 0.0..=20.0)
                               .suffix("%")
                               .text("Y"));
                    });
                }
            }
        });

        // Rest of the update function remains the same
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Plot area — pan with mouse, zoom with scroll");
            ui.add_space(6.0);

            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.set_width(200.0);
                    ui.label("Datasets:");

                    let mut remove_index: Option<usize> = None;
                    for (i, ds) in self.datasets.iter().enumerate() {
                        ui.horizontal(|ui| {
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
                    if self.show_legend {
                        plot = plot.legend(Legend::default());
                    }

                    plot.show(ui, |plot_ui| {
                        for ds in &self.datasets {
                            let line = Line::new(PlotPoints::new(ds.points.clone())).name(&ds.name);
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
        let (min_x, max_x, min_y, max_y) = if let Some(config) = axis_config {
            calculate_custom_bounds(datasets, &config)?
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

        // Draw axis labels
        draw_axis_labels(
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
        );

        // Draw datasets
        let line_colors = if dark_mode {
            [
                [100, 149, 237], [255, 165, 0], [50, 205, 50], [255, 99, 71],
                [186, 85, 211], [210, 180, 140], [255, 182, 193], [192, 192, 192],
            ]
        } else {
            [
                [31, 120, 180], [255, 127, 14], [44, 160, 44], [214, 39, 40],
                [148, 103, 189], [140, 86, 75], [227, 119, 194], [127, 127, 127],
            ]
        };

        for (dataset_idx, dataset) in datasets.iter().enumerate() {
            if dataset.points.is_empty() {
                continue;
            }
            let color_idx = dataset_idx % line_colors.len();
            let color = line_colors[color_idx];
            let rgb_color = image::Rgb(color);
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
    // Get data bounds as fallback
    let (data_min_x, data_max_x, data_min_y, data_max_y) = get_data_bounds(datasets)
        .ok_or("No data available")?;

    // Use custom values if provided, otherwise use data bounds
    let base_min_x = config.x_min.unwrap_or(data_min_x);
    let base_max_x = config.x_max.unwrap_or(data_max_x);
    let base_min_y = config.y_min.unwrap_or(data_min_y);
    let base_max_y = config.y_max.unwrap_or(data_max_y);

    // Apply padding
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

    // Handle edge cases
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

    // Smart padding
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



fn draw_axis_labels(
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
) {
    // X-axis ticks and labels - reduce number of ticks for large numbers
    let num_x_ticks = 6; // Reduced from 8 to give more space
    for i in 0..=num_x_ticks {
        let x_pos = margin_left + (i * plot_width / num_x_ticks);
        let tick_y = height - margin_bottom;
        
        // Draw tick mark
        for dy in 0..8 {
            if tick_y + dy < height {
                img.put_pixel(x_pos, tick_y + dy, color);
            }
        }
        
        let data_x = min_x + (max_x - min_x) * (i as f64 / num_x_ticks as f64);
        
        // Center the label under the tick mark with better spacing
        let text = format_number(data_x);
        let text_width = text.len() as u32 * 6; // 6 pixels per character
        let label_x = if x_pos >= text_width / 2 {
            x_pos - text_width / 2
        } else {
            0
        };
        
        // Move labels further down to avoid overlap with axis
        draw_number_pixels(img, label_x, tick_y + 20, data_x, color);
    }
    
    // Y-axis ticks and labels
    let num_y_ticks = 6;
    for i in 0..=num_y_ticks {
        let y_pos = height - margin_bottom - (i * plot_height / num_y_ticks);
        let tick_x = margin_left;
        
        // Draw tick mark
        for dx in 0..8 {
            if tick_x >= dx {
                img.put_pixel(tick_x - dx, y_pos, color);
            }
        }
        
        let data_y = min_y + (max_y - min_y) * (i as f64 / num_y_ticks as f64);
        
        // Right-align the Y labels to the left of the axis with more space
        let text = format_number(data_y);
        let text_width = text.len() as u32 * 6;
        let label_x = if tick_x >= text_width + 15 {
            tick_x - text_width - 15 // Increased spacing from 10 to 15
        } else {
            0
        };
        
        draw_number_pixels(img, label_x, y_pos.saturating_sub(3), data_y, color);
    }
}
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
        '0' => [
            0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110,
        ],
        '1' => [
            0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110,
        ],
        '2' => [
            0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111,
        ],
        '3' => [
            0b11111, 0b00010, 0b00100, 0b00110, 0b00001, 0b10001, 0b01110,
        ],
        '4' => [
            0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010,
        ],
        '5' => [
            0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110,
        ],
        '6' => [
            0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110,
        ],
        '7' => [
            0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000,
        ],
        '8' => [
            0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110,
        ],
        '9' => [
            0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100,
        ],
        '.' => [
            0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100,
        ],
        '-' => [
            0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000,
        ],
        'K' => [
            0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001,
        ],
        'M' => [
            0b10001, 0b11011, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001,
        ],
        'e' => [
            0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b10001, 0b01110,
        ],
        _ => [
            0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000,
        ],
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

fn draw_line(img: &mut image::RgbImage, x0: u32, y0: u32, x1: u32, y1: u32, color: image::Rgb<u8>) {
    let dx = (x1 as i32 - x0 as i32).abs();
    let dy = (y1 as i32 - y0 as i32).abs();
    let sx = if x0 < x1 { 1 } else { -1 };
    let sy = if y0 < y1 { 1 } else { -1 };
    let mut err = dx - dy;
    let mut x = x0 as i32;
    let mut y = y0 as i32;

    loop {
        if x >= 0 && y >= 0 && (x as u32) < img.width() && (y as u32) < img.height() {
            img.put_pixel(x as u32, y as u32, color);
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

fn save_screenshot_with_dialog(
    screenshot: &egui::ColorImage,
) -> Result<(), Box<dyn std::error::Error>> {
    if let Some(path) = rfd::FileDialog::new()
        .add_filter("PNG Image", &["png"])
        .set_file_name("plot.png")
        .save_file()
    {
        let pixels = screenshot
            .pixels
            .iter()
            .flat_map(|color| [color.r(), color.g(), color.b(), color.a()])
            .collect::<Vec<u8>>();

        let img = image::RgbaImage::from_raw(
            screenshot.size[0] as u32,
            screenshot.size[1] as u32,
            pixels,
        )
        .ok_or("Failed to create image")?;

        img.save(&path)?;
        println!("Plot successfully saved as: {}", path.display());
    }

    Ok(())
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


fn format_number(value: f64) -> String {
    let abs_value = value.abs();
    
    // Debug: print the value being formatted
    println!("Formatting value: {}", value);
    
    if abs_value >= 1_000_000.0 {
        // For millions: 1.5M, 2.3M, etc.
        let m_value = value / 1_000_000.0;
        format!("{:.1}M", m_value)
    } else if abs_value >= 100_000.0 {
        // For hundreds of thousands: 250K, 437K, etc.
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 10_000.0 {
        // For tens of thousands: 87K, 43K, etc.
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 1000.0 {
        // For thousands: 1000, 5000, etc.
        format!("{:.0}", value)
    } else if abs_value >= 1.0 {
        // For regular numbers: 50, 123, etc.
        if value.fract().abs() < 0.01 {
            format!("{:.0}", value)
        } else {
            format!("{:.1}", value)
        }
    } else if abs_value >= 0.01 {
        // For small decimals: 0.17, 0.29, etc.
        format!("{:.3}", value)
    } else if abs_value > f64::EPSILON {
        format!("{:.4}", value)
    } else {
        "0".to_string()
    }
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
                    app.datasets.push(Dataset {
                        name: file.clone(),
                        points,
                    });
                    app.next_name_index += 1;
                }
            }

            Box::new(app)
        }),
    )
    .unwrap();
}
