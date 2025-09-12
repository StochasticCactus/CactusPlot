import dearpygui.dearpygui as dpg
import numpy as np
import math
import csv
from typing import List, Tuple, Dict

class XMGracePlotter:
    def __init__(self):
        self.data_sets = {}
        self.current_set = 0
        self.plot_colors = [
            [255, 0, 0, 255],    # Red
            [0, 255, 0, 255],    # Green  
            [0, 0, 255, 255],    # Blue
            [255, 255, 0, 255],  # Yellow
            [255, 0, 255, 255],  # Magenta
            [0, 255, 255, 255],  # Cyan
            [128, 128, 128, 255] # Gray
        ]
        self.line_styles = ["solid", "dashed", "dotted"]
        
    def add_data_set(self, x_data: List[float], y_data: List[float], label: str = ""):
        """Add a new data set to the plot"""
        set_id = len(self.data_sets)
        self.data_sets[set_id] = {
            'x': x_data,
            'y': y_data,
            'label': label or f"Set {set_id}",
            'visible': True,
            'color': self.plot_colors[set_id % len(self.plot_colors)],
            'line_style': 'solid',
            'marker_style': 'none'
        }
        return set_id
    
    def update_plot(self):
        """Update the main plot with current data sets"""
        if not dpg.does_item_exist("y_axis"):
            return  # Plot axes not created yet
            
        # Remove existing series
        for set_id in list(self.data_sets.keys()):
            if dpg.does_item_exist(f"series_{set_id}"):
                dpg.delete_item(f"series_{set_id}")
            
        # Add current series
        for set_id, data in self.data_sets.items():
            if data['visible'] and len(data['x']) > 0:
                dpg.add_line_series(
                    data['x'], data['y'], 
                    label=data['label'],
                    parent="y_axis",
                    tag=f"series_{set_id}"
                )
                if dpg.does_item_exist(f"theme_{set_id}"):
                    dpg.set_item_theme(f"series_{set_id}", f"theme_{set_id}")
    
    def create_themes(self):
        """Create color themes for different data sets"""
        for i, color in enumerate(self.plot_colors):
            with dpg.theme(tag=f"theme_{i}"):
                with dpg.theme_component(dpg.mvLineSeries):
                    dpg.add_theme_color(dpg.mvPlotCol_Line, color, category=dpg.mvThemeCat_Plots)
    
    def load_data_callback(self, sender, app_data):
        """Callback for loading data from file"""
        try:
            file_path = app_data['file_path_name']
            x_data = []
            y_data = []
            
            with open(file_path, 'r') as file:
                try:
                    reader = np.loadtxt(file, comments=["@", "#"])
                except Exception as E:
                    print(E)
            
                for line_num, row in enumerate(reader):
                    if len(row) >= 2:
                        try:
                            # Try to parse as space or tab separated values first
                            if len(row) == 1:
                                values = row[0].split()
                            else:
                                values = row
                            
                            if len(values) >= 2:
                                x_val = float(values[0])
                                y_val = float(values[1])
                                x_data.append(x_val)
                                y_data.append(y_val)
                        except ValueError:
                            continue  # Skip invalid lines

            if x_data and y_data:
                filename = file_path.split('/')[-1].split('\\')[-1]
                set_id = self.add_data_set(x_data, y_data, filename)
                self.update_plot()
                self.update_data_list()
                dpg.set_value("status_text", f"Loaded {len(x_data)} points from {filename}")
            else:
                dpg.set_value("status_text", "Error: No valid data found in file")
                
        except Exception as e:
            dpg.set_value("status_text", f"Error loading file: {str(e)}")
    
    def save_data_callback(self, sender, app_data):
        """Callback for saving current data to file"""
        try:
            file_path = app_data['file_path_name']
            if self.current_set in self.data_sets:
                data = self.data_sets[self.current_set]
                with open(file_path, 'w', newline='') as file:
                    writer = csv.writer(file, delimiter='\t')
                    for x, y in zip(data['x'], data['y']):
                        writer.writerow([x, y])
                dpg.set_value("status_text", f"Data saved to {file_path}")
            else:
                dpg.set_value("status_text", "No data set selected to save")
        except Exception as e:
            dpg.set_value("status_text", f"Error saving file: {str(e)}")
    
    def generate_function_data(self):
        """Generate data from mathematical function"""
        func_str = dpg.get_value("function_input")
        x_min = dpg.get_value("x_min_input")
        x_max = dpg.get_value("x_max_input")
        n_points = dpg.get_value("n_points_input")
        
        try:
            x_data = np.linspace(x_min, x_max, n_points)
            y_data = []
            
            for x in x_data:
                # Replace common function names for eval
                func_eval = func_str
                for func_name in ['sin', 'cos', 'tan', 'exp', 'log', 'sqrt']:
                    func_eval = func_eval.replace(func_name, f'math.{func_name}')
                func_eval = func_eval.replace('pi', 'math.pi')
                func_eval = func_eval.replace('x', str(x))
                
                y_val = eval(func_eval)
                y_data.append(y_val)
            
            set_id = self.add_data_set(x_data.tolist(), y_data, f"f(x) = {func_str}")
            self.update_plot()
            self.update_data_list()
            dpg.set_value("status_text", f"Generated function: {func_str}")
            
        except Exception as e:
            dpg.set_value("status_text", f"Error generating function: {str(e)}")
    
    def update_data_list(self):
        """Update the data sets list"""
        if dpg.does_item_exist("data_listbox"):
            items = []
            for set_id, data in self.data_sets.items():
                status = "✓" if data['visible'] else "✗"
                items.append(f"{status} {data['label']}")
            
            dpg.configure_item("data_listbox", items=items)
    
    def toggle_data_visibility(self):
        """Toggle visibility of selected data set"""
        selection = dpg.get_value("data_listbox")
        if selection >= 0 and selection < len(self.data_sets):
            self.data_sets[selection]['visible'] = not self.data_sets[selection]['visible']
            self.current_set = selection
            self.update_plot()
            self.update_data_list()
    
    def delete_data_set(self):
        """Delete selected data set"""
        selection = dpg.get_value("data_listbox")
        if selection >= 0 and selection in self.data_sets:
            del self.data_sets[selection]
            # Reorganize data sets
            new_data_sets = {}
            for i, (old_id, data) in enumerate(self.data_sets.items()):
                new_data_sets[i] = data
            self.data_sets = new_data_sets
            self.update_plot()
            self.update_data_list()
            dpg.set_value("status_text", "Data set deleted")
    
    def auto_scale(self):
        """Auto-scale the plot axes"""
        if not self.data_sets:
            return
            
        all_x = []
        all_y = []
        for data in self.data_sets.values():
            if data['visible']:
                all_x.extend(data['x'])
                all_y.extend(data['y'])
        
        if all_x and all_y:
            x_margin = (max(all_x) - min(all_x)) * 0.1
            y_margin = (max(all_y) - min(all_y)) * 0.1
            
            dpg.set_axis_limits("x_axis", 
                              min(all_x) - x_margin, 
                              max(all_x) + x_margin)
            dpg.set_axis_limits("y_axis", 
                              min(all_y) - y_margin, 
                              max(all_y) + y_margin)
    
    def add_sample_data(self):
        """Add sample data after GUI is initialized"""
        x_sample = np.linspace(0, 10, 50)
        y_sample1 = np.sin(x_sample)
        y_sample2 = np.cos(x_sample)
        
        self.add_data_set(x_sample.tolist(), y_sample1.tolist(), "sin(x)")
        self.add_data_set(x_sample.tolist(), y_sample2.tolist(), "cos(x)")
        self.update_plot()
        self.update_data_list()
        self.auto_scale()
    
    def setup_gui(self):
        """Setup the main GUI"""
        dpg.create_context()
        
        # Create themes
        self.create_themes()
        
        # File dialogs
        with dpg.file_dialog(directory_selector=False, show=False, callback=self.load_data_callback, 
                           tag="file_dialog_load", width=800, height=400):
            dpg.add_file_extension(".*")
            dpg.add_file_extension(".dat", color=[255, 255, 0, 255])
            dpg.add_file_extension(".txt", color=[0, 255, 255, 255])
            dpg.add_file_extension(".csv", color=[255, 0, 255, 255])
            dpg.add_file_extension(".xvg", color=[255, 0, 255, 255]) 

        with dpg.file_dialog(directory_selector=False, show=False, callback=self.save_data_callback,
                           tag="file_dialog_save", width=700, height=400, default_filename="data.dat"):
            dpg.add_file_extension(".dat", color=[255, 255, 0, 255])
            dpg.add_file_extension(".txt", color=[0, 255, 255, 255])
            dpg.add_file_extension(".csv", color=[255, 0, 255, 255])
            dpg.add_file_extension(".xvg", color=[255, 0, 255, 255])         
        # Main window
        with dpg.window(tag="main_window", label="XMGrace-style Plotter"):
            
            # Menu bar
            with dpg.menu_bar():
                with dpg.menu(label="File"):
                    dpg.add_menu_item(label="Load Data", callback=lambda: dpg.show_item("file_dialog_load"))
                    dpg.add_menu_item(label="Save Data", callback=lambda: dpg.show_item("file_dialog_save"))
                    dpg.add_separator()
                    dpg.add_menu_item(label="Exit", callback=dpg.stop_dearpygui)
                
                with dpg.menu(label="Data"):
                    dpg.add_menu_item(label="Generate Function", callback=lambda: dpg.show_item("function_window"))
                    dpg.add_menu_item(label="Auto Scale", callback=self.auto_scale)
            
            # Main layout
            with dpg.group(horizontal=True):
                
                # Left panel - Controls
                with dpg.group(width=200):
                    dpg.add_text("Data Sets")
                    dpg.add_listbox([], tag="data_listbox", width=180, num_items=8,
                                  callback=lambda: self.toggle_data_visibility())
                    
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Toggle Visibility", callback=self.toggle_data_visibility)
                        dpg.add_button(label="Delete", callback=self.delete_data_set)
                    
                    dpg.add_separator()
                    
                    dpg.add_text("Plot Controls")
                    dpg.add_button(label="Auto Scale", callback=self.auto_scale, width=180)
                    
                    dpg.add_separator()
                    
                    dpg.add_text("Quick Function Generator")
                    dpg.add_input_text(label="f(x) = ", tag="function_input", 
                                      default_value="sin(x)", width=100)
                    
                    with dpg.group(horizontal=True):
                        dpg.add_input_double(label="X min", tag="x_min_input", 
                                           default_value=-10.0, width=80)
                        dpg.add_input_double(label="X max", tag="x_max_input", 
                                           default_value=10.0, width=80)
                    
                    dpg.add_input_int(label="Points", tag="n_points_input", 
                                    default_value=100, width=80)
                    dpg.add_button(label="Generate", callback=self.generate_function_data, width=280)
                
                # Right panel - Plot
                with dpg.group():
                    with dpg.plot(label="Plot", height=800, width=800, tag="main_plot"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="X", tag="x_axis")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Y", tag="y_axis")
            
            # Status bar
            dpg.add_separator()
            dpg.add_text("Ready", tag="status_text")
        
        # Function generator window
        with dpg.window(label="Function Generator", modal=True, show=False, 
                       tag="function_window", width=400, height=300):
            dpg.add_text("Generate data from mathematical functions")
            dpg.add_separator()
            
            dpg.add_text("Supported functions: sin, cos, tan, exp, log, sqrt, pi")
            dpg.add_text("Example: sin(x) + cos(2*x)")
            
            dpg.add_separator()
            
            dpg.add_input_text(label="Function f(x) = ", default_value="sin(x)", width=300)
            
            with dpg.group(horizontal=True):
                dpg.add_input_double(label="X min", default_value=-10.0, width=120)
                dpg.add_input_double(label="X max", default_value=10.0, width=120)
            
            dpg.add_input_int(label="Number of points", default_value=100, width=300)
            
            dpg.add_separator()
            
            with dpg.group(horizontal=True):
                dpg.add_button(label="Generate", callback=self.generate_function_data)
                dpg.add_button(label="Cancel", callback=lambda: dpg.hide_item("function_window"))
        
        dpg.create_viewport(title="XMGrace-style Plotter", width=1200, height=800)
        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.set_primary_window("main_window", True)
    
    def run(self):
        """Run the application"""
        self.setup_gui()
        
        # Add sample data after the first frame
        def init_data():
            self.add_sample_data()
        
        # Schedule initialization for next frame
        dpg.set_frame_callback(1, init_data)
        
        dpg.start_dearpygui()
        dpg.destroy_context()

# Run the application
if __name__ == "__main__":
    app = XMGracePlotter()
    app.run()
