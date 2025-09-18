use clap::Parser;
use eframe::{egui, App, Frame};
use egui_plot::{Legend, Line, Plot, PlotPoints};
use rand::prelude::*;
use rand::thread_rng;
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
        }
    }
}

impl App for PlotterApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut Frame) {
        if self.dark_mode {
            ctx.set_visuals(egui::Visuals::dark())
        } else {
            ctx.set_visuals(egui::Visuals::light());
        }

        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            ui.horizontal(|ui| {
                if ui.button("Open CSV").clicked() {
                    if let Some(path) = pick_csv_file() {
                        match load_csv_points(&path) {
                            Ok(points) => {
                                let name = format!("data{}", self.next_name_index);
                                self.next_name_index += 1;
                                self.datasets.push(Dataset { name, points });
                                self.error_message = None;
                            }
                            Err(e) => {
                                self.error_message = Some(format!("Failed to load CSV: {}", e));
                            }
                        }
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

                    let visuals = ui.style().interact(&response);
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
                    // Generate truly random dataset/
                    let mut rng = thread_rng();
                    let mut pts = Vec::new();
                    let n = 120usize;
                    for i in 0..n {
                        let x = i as f64 / n as f64 * 10.0;
                        let y = rng.gen_range(-2.0..2.0); // Random y values between -2 and 2
                        pts.push([x, y]);
                    }
                    let name = format!("random{}", self.next_name_index);
                    self.next_name_index += 1;
                    self.datasets.push(Dataset { name, points: pts });
                }
            });

            // Show error message if any
            if let Some(ref error) = self.error_message {
                ui.colored_label(egui::Color32::RED, error);
            }
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Plot area — pan with mouse, zoom with scroll");
            ui.add_space(6.0);

            // Use horizontal layout to put controls on left, plot on right
            ui.horizontal(|ui| {
                // Left side controls
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

                // Plot on the right - takes remaining space
                ui.vertical(|ui| {
                    let mut plot = Plot::new("main_plot")
                        .height(500.0)
                        .width(ui.available_width())
                        .show_axes([true, true])
                        .show_grid([false, false]);
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

// Helper: open a file dialog (CSV) using rfd
fn pick_csv_file() -> Option<PathBuf> {
    rfd::FileDialog::new()
        .add_filter("csv", &["csv"])
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

                    app.next_name_index += 1
                }
            }
            Box::new(app)
        }),
    )
    .unwrap();
}
