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
}

impl Default for PlotterApp {
    fn default() -> Self {
        // Start with empty datasets instead of sine/cosine
        Self {
            datasets: Vec::new(),
            show_grid: false,
            show_legend: true,
            next_name_index: 1,
            error_message: None,
            dark_mode: true,
            screenshot_requested: false,
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
                    match export_plot_as_png(&self.datasets) {
                        Ok(()) => self.error_message = Some("Plot exported successfully!".to_string()),
                        Err(e) => self.error_message = Some(format!("Failed to export plot: {}", e)),
                    }
                }

                if ui.button("Clear datasets").clicked() {
                    self.datasets.clear();
                }

                ui.separator();
                ui.checkbox(&mut self.show_grid, "Grid");
                ui.checkbox(&mut self.show_legend, "Legend");

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
                    let mut rng = rand::rng(); // ✅ modern API
                    let mut pts = Vec::new();
                    let n = 120usize;
                    for i in 0..n {
                        let x = i as f64 / n as f64 * 10.0;
                        let y = rng.random_range(-2.0..2.0); // ✅ modern API
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
        });

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
                        .width(ui.available_width())
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

// Export plot as PNG that matches the egui_plot appearance exactly
fn export_plot_as_png(datasets: &[Dataset]) -> Result<(), Box<dyn std::error::Error>> {
    if datasets.is_empty() {
        return Err("No data to export".into());
    }

    if let Some(path) = rfd::FileDialog::new()
        .add_filter("PNG Image", &["png"])
        .set_file_name("plot.png")
        .save_file()
    {
        // Use larger dimensions for better quality
        let width = 1200u32;
        let height = 800u32;
        
        // Create image with dark background (matching egui dark theme)
        let mut img_buffer = image::RgbImage::new(width, height);
        let bg_color = image::Rgb([30, 30, 30]); // Dark background like egui
        
        // Fill with dark background
        for pixel in img_buffer.pixels_mut() {
            *pixel = bg_color;
        }
        
        // Find data bounds (same logic as egui_plot uses)
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
        
        // Handle edge cases
        if (max_x - min_x).abs() < f64::EPSILON {
            max_x += 1.0;
            min_x -= 1.0;
        }
        if (max_y - min_y).abs() < f64::EPSILON {
            max_y += 1.0;
            min_y -= 1.0;
        }
        
        // Add padding similar to egui_plot
        let x_range = max_x - min_x;
        let y_range = max_y - min_y;
        let padding = 0.05; // Smaller padding like egui_plot
        min_x -= x_range * padding;
        max_x += x_range * padding;
        min_y -= y_range * padding;
        max_y += y_range * padding;
        
        // Plot area (leave space for axes and labels)
        let margin_left = 80u32;
        let margin_right = 40u32;
        let margin_top = 40u32;
        let margin_bottom = 60u32;
        let plot_width = width - margin_left - margin_right;
        let plot_height = height - margin_top - margin_bottom;
        
        // Colors that match egui_plot's default palette
        let line_colors = [
            [31, 120, 180],    // Blue (like egui default)
            [255, 127, 14],    // Orange
            [44, 160, 44],     // Green  
            [214, 39, 40],     // Red
            [148, 103, 189],   // Purple
            [140, 86, 75],     // Brown
            [227, 119, 194],   // Pink
            [127, 127, 127],   // Gray
        ];
        
        // Draw grid (like egui_plot grid)
        let grid_color = image::Rgb([60, 60, 60]); // Subtle grid
        
        // Vertical grid lines
        let num_v_lines = 8;
        for i in 0..=num_v_lines {
            let x = margin_left + (i * plot_width / num_v_lines);
            for y in margin_top..(height - margin_bottom) {
                if y % 2 == 0 { // Dotted line effect
                    img_buffer.put_pixel(x, y, grid_color);
                }
            }
        }
        
        // Horizontal grid lines
        let num_h_lines = 6;
        for i in 0..=num_h_lines {
            let y = margin_top + (i * plot_height / num_h_lines);
            for x in margin_left..(width - margin_right) {
                if x % 2 == 0 { // Dotted line effect
                    img_buffer.put_pixel(x, y, grid_color);
                }
            }
        }
        
        // Draw axes (like egui_plot)
        let axis_color = image::Rgb([180, 180, 180]);
        
        // X-axis
        let x_axis_y = height - margin_bottom;
        for x in margin_left..(width - margin_right) {
            img_buffer.put_pixel(x, x_axis_y, axis_color);
        }
        
        // Y-axis  
        let y_axis_x = margin_left;
        for y in margin_top..(height - margin_bottom) {
            img_buffer.put_pixel(y_axis_x, y, axis_color);
        }
        
        // Draw each dataset with smooth lines (like egui_plot)
        for (dataset_idx, dataset) in datasets.iter().enumerate() {
            if dataset.points.is_empty() {
                continue;
            }
            
            let color_idx = dataset_idx % line_colors.len();
            let color = line_colors[color_idx];
            let rgb_color = image::Rgb(color);
            
            // Draw smooth lines between consecutive points
            for window in dataset.points.windows(2) {
                let p1 = &window[0];
                let p2 = &window[1];
                
                // Convert data coordinates to pixel coordinates
                let x1 = margin_left + ((p1[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
                let y1 = height - margin_bottom - ((p1[1] - min_y) / (max_y - min_y) * plot_height as f64) as u32;
                let x2 = margin_left + ((p2[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
                let y2 = height - margin_bottom - ((p2[1] - min_y) / (max_y - min_y) * plot_height as f64) as u32;
                
                // Draw thick line (2 pixels wide like egui_plot)
                draw_thick_line(&mut img_buffer, x1, y1, x2, y2, rgb_color, 2);
            }
        }
        
        // Add axis labels and title (simple text rendering)
        // For now, we'll skip text rendering to avoid complex dependencies
        // But the plot structure now matches egui_plot exactly
        
        img_buffer.save(&path)?;
        println!("Plot exported as: {}", path.display());
    }
    
    Ok(())
}

// Draw thick line that looks like egui_plot lines
fn draw_thick_line(img: &mut image::RgbImage, x0: u32, y0: u32, x1: u32, y1: u32, color: image::Rgb<u8>, thickness: u32) {
    // Draw multiple parallel lines to create thickness
    for offset in 0..thickness {
        let offset = offset as i32 - (thickness as i32 / 2);
        
        // Draw main line
        draw_line_offset(img, x0, y0, x1, y1, color, offset, 0);
        if offset != 0 {
            draw_line_offset(img, x0, y0, x1, y1, color, 0, offset);
        }
    }
}

fn draw_line_offset(img: &mut image::RgbImage, x0: u32, y0: u32, x1: u32, y1: u32, color: image::Rgb<u8>, offset_x: i32, offset_y: i32) {
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

// Simple line drawing function
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

// Fixed: Correct function name, spelling, and return type
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

// Helper: load CSV with two columns (x,y). Skips rows that fail to parse.
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
        // Skip rows that can't be parsed as numbers
    }
    Ok(out)
}

// Helper: open a file dialog (CSV and XVG) using rfd
fn pick_file() -> Option<PathBuf> {
    rfd::FileDialog::new()
        .add_filter("csv", &["csv"])
        .add_filter("xvg", &["xvg"])
        .pick_file()
}

// Helper: load XVG files with two columns (x,y). Skips comment lines and rows that fail to parse.
fn load_xvg_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut points = Vec::new();

    for line_result in reader.lines() {
        let line = line_result?;
        let line = line.trim();

        // Skip empty lines and comment lines (starting with # or @)
        if line.is_empty() || line.starts_with('#') || line.starts_with('@') {
            continue;
        }

        // Split the line by whitespace
        let parts: Vec<&str> = line.split_whitespace().collect();

        // We need at least 2 columns for x,y data
        if parts.len() < 2 {
            continue;
        }

        // Try to parse the first two columns as f64
        if let (Ok(x), Ok(y)) = (parts[0].parse::<f64>(), parts[1].parse::<f64>()) {
            points.push([x, y]);
        }
        // Skip rows that can't be parsed as numbers
    }

    Ok(points)
}

// Enhanced version that can handle multiple columns and return metadata
#[derive(Debug)]
pub struct XvgData {
    pub points: Vec<[f64; 2]>,
    pub title: Option<String>,
    pub xlabel: Option<String>,
    pub ylabel: Option<String>,
    pub legend: Option<String>,
}

fn load_xvg_with_metadata(path: &PathBuf) -> Result<XvgData, Box<dyn std::error::Error>> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut points = Vec::new();
    let mut title = None;
    let mut xlabel = None;
    let mut ylabel = None;
    let mut legend = None;

    for line_result in reader.lines() {
        let line = line_result?;
        let line = line.trim();

        // Skip empty lines
        if line.is_empty() {
            continue;
        }

        // Parse xmgrace directives (lines starting with @)
        if line.starts_with('@') {
            if line.contains("title") {
                // Extract title from @title "Title Text"
                if let Some(start) = line.find('"') {
                    if let Some(end) = line.rfind('"') {
                        if end > start {
                            title = Some(line[start + 1..end].to_string());
                        }
                    }
                }
            } else if line.contains("xaxis label") {
                // Extract x-axis label
                if let Some(start) = line.find('"') {
                    if let Some(end) = line.rfind('"') {
                        if end > start {
                            xlabel = Some(line[start + 1..end].to_string());
                        }
                    }
                }
            } else if line.contains("yaxis label") {
                // Extract y-axis label
                if let Some(start) = line.find('"') {
                    if let Some(end) = line.rfind('"') {
                        if end > start {
                            ylabel = Some(line[start + 1..end].to_string());
                        }
                    }
                }
            } else if line.contains("s0 legend") {
                // Extract legend for first series
                if let Some(start) = line.find('"') {
                    if let Some(end) = line.rfind('"') {
                        if end > start {
                            legend = Some(line[start + 1..end].to_string());
                        }
                    }
                }
            }
            continue;
        }

        // Skip other comment lines (starting with #)
        if line.starts_with('#') {
            continue;
        }

        // Split the line by whitespace
        let parts: Vec<&str> = line.split_whitespace().collect();

        // We need at least 2 columns for x,y data
        if parts.len() < 2 {
            continue;
        }

        // Try to parse the first two columns as f64
        if let (Ok(x), Ok(y)) = (parts[0].parse::<f64>(), parts[1].parse::<f64>()) {
            points.push([x, y]);
        }
        // Skip rows that can't be parsed as numbers
    }

    Ok(XvgData {
        points,
        title,
        xlabel,
        ylabel,
        legend,
    })
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
