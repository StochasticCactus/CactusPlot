import dearpygui.dearpygui as dpg
import numpy as np
import math
import csv
from typing import List, Tuple, Dict

class XMGracePlotter:
    def __init__(self):
        self.data_sets = {}
        self.current_set = 0
        # All matplotlib named colors converted to RGBA [0-255] format
        self.matplotlib_colors = {
            'aliceblue': [240, 248, 255, 255], 'antiquewhite': [250, 235, 215, 255],
            'aqua': [0, 255, 255, 255], 'aquamarine': [127, 255, 212, 255],
            'azure': [240, 255, 255, 255], 'beige': [245, 245, 220, 255],
            'bisque': [255, 228, 196, 255], 'black': [0, 0, 0, 255],
            'blanchedalmond': [255, 235, 205, 255], 'blue': [0, 0, 255, 255],
            'blueviolet': [138, 43, 226, 255], 'brown': [165, 42, 42, 255],
            'burlywood': [222, 184, 135, 255], 'cadetblue': [95, 158, 160, 255],
            'chartreuse': [127, 255, 0, 255], 'chocolate': [210, 105, 30, 255],
            'coral': [255, 127, 80, 255], 'cornflowerblue': [100, 149, 237, 255],
            'cornsilk': [255, 248, 220, 255], 'crimson': [220, 20, 60, 255],
            'cyan': [0, 255, 255, 255], 'darkblue': [0, 0, 139, 255],
            'darkcyan': [0, 139, 139, 255], 'darkgoldenrod': [184, 134, 11, 255],
            'darkgray': [169, 169, 169, 255], 'darkgreen': [0, 100, 0, 255],
            'darkkhaki': [189, 183, 107, 255], 'darkmagenta': [139, 0, 139, 255],
            'darkolivegreen': [85, 107, 47, 255], 'darkorange': [255, 140, 0, 255],
            'darkorchid': [153, 50, 204, 255], 'darkred': [139, 0, 0, 255],
            'darksalmon': [233, 150, 122, 255], 'darkseagreen': [143, 188, 143, 255],
            'darkslateblue': [72, 61, 139, 255], 'darkslategray': [47, 79, 79, 255],
            'darkturquoise': [0, 206, 209, 255], 'darkviolet': [148, 0, 211, 255],
            'deeppink': [255, 20, 147, 255], 'deepskyblue': [0, 191, 255, 255],
            'dimgray': [105, 105, 105, 255], 'dodgerblue': [30, 144, 255, 255],
            'firebrick': [178, 34, 34, 255], 'floralwhite': [255, 250, 240, 255],
            'forestgreen': [34, 139, 34, 255], 'fuchsia': [255, 0, 255, 255],
            'gainsboro': [220, 220, 220, 255], 'ghostwhite': [248, 248, 255, 255],
            'gold': [255, 215, 0, 255], 'goldenrod': [218, 165, 32, 255],
            'gray': [128, 128, 128, 255], 'green': [0, 128, 0, 255],
            'greenyellow': [173, 255, 47, 255], 'honeydew': [240, 255, 240, 255],
            'hotpink': [255, 105, 180, 255], 'indianred': [205, 92, 92, 255],
            'indigo': [75, 0, 130, 255], 'ivory': [255, 255, 240, 255],
            'khaki': [240, 230, 140, 255], 'lavender': [230, 230, 250, 255],
            'lavenderblush': [255, 240, 245, 255], 'lawngreen': [124, 252, 0, 255],
            'lemonchiffon': [255, 250, 205, 255], 'lightblue': [173, 216, 230, 255],
            'lightcoral': [240, 128, 128, 255], 'lightcyan': [224, 255, 255, 255],
            'lightgoldenrodyellow': [250, 250, 210, 255], 'lightgray': [211, 211, 211, 255],
            'lightgreen': [144, 238, 144, 255], 'lightpink': [255, 182, 193, 255],
            'lightsalmon': [255, 160, 122, 255], 'lightseagreen': [32, 178, 170, 255],
            'lightskyblue': [135, 206, 250, 255], 'lightslategray': [119, 136, 153, 255],
            'lightsteelblue': [176, 196, 222, 255], 'lightyellow': [255, 255, 224, 255],
            'lime': [0, 255, 0, 255], 'limegreen': [50, 205, 50, 255],
            'linen': [250, 240, 230, 255], 'magenta': [255, 0, 255, 255],
            'maroon': [128, 0, 0, 255], 'mediumaquamarine': [102, 205, 170, 255],
            'mediumblue': [0, 0, 205, 255], 'mediumorchid': [186, 85, 211, 255],
            'mediumpurple': [147, 112, 219, 255], 'mediumseagreen': [60, 179, 113, 255],
            'mediumslateblue': [123, 104, 238, 255], 'mediumspringgreen': [0, 250, 154, 255],
            'mediumturquoise': [72, 209, 204, 255], 'mediumvioletred': [199, 21, 133, 255],
            'midnightblue': [25, 25, 112, 255], 'mintcream': [245, 255, 250, 255],
            'mistyrose': [255, 228, 225, 255], 'moccasin': [255, 228, 181, 255],
            'navajowhite': [255, 222, 173, 255], 'navy': [0, 0, 128, 255],
            'oldlace': [253, 245, 230, 255], 'olive': [128, 128, 0, 255],
            'olivedrab': [107, 142, 35, 255], 'orange': [255, 165, 0, 255],
            'orangered': [255, 69, 0, 255], 'orchid': [218, 112, 214, 255],
            'palegoldenrod': [238, 232, 170, 255], 'palegreen': [152, 251, 152, 255],
            'paleturquoise': [175, 238, 238, 255], 'palevioletred': [219, 112, 147, 255],
            'papayawhip': [255, 239, 213, 255], 'peachpuff': [255, 218, 185, 255],
            'peru': [205, 133, 63, 255], 'pink': [255, 192, 203, 255],
            'plum': [221, 160, 221, 255], 'powderblue': [176, 224, 230, 255],
            'purple': [128, 0, 128, 255], 'red': [255, 0, 0, 255],
            'rosybrown': [188, 143, 143, 255], 'royalblue': [65, 105, 225, 255],
            'saddlebrown': [139, 69, 19, 255], 'salmon': [250, 128, 114, 255],
            'sandybrown': [244, 164, 96, 255], 'seagreen': [46, 139, 87, 255],
            'seashell': [255, 245, 238, 255], 'sienna': [160, 82, 45, 255],
            'silver': [192, 192, 192, 255], 'skyblue': [135, 206, 235, 255],
            'slateblue': [106, 90, 205, 255], 'slategray': [112, 128, 144, 255],
            'snow': [255, 250, 250, 255], 'springgreen': [0, 255, 127, 255],
            'steelblue': [70, 130, 180, 255], 'tan': [210, 180, 140, 255],
            'teal': [0, 128, 128, 255], 'thistle': [216, 191, 216, 255],
            'tomato': [255, 99, 71, 255], 'turquoise': [64, 224, 208, 255],
            'violet': [238, 130, 238, 255], 'wheat': [245, 222, 179, 255],
            'white': [255, 255, 255, 255], 'whitesmoke': [245, 245, 245, 255],
            'yellow': [255, 255, 0, 255], 'yellowgreen': [154, 205, 50, 255]
        }
        # Select good contrasting colors for plotting (excluding very light colors)
        self.plot_colors = [
            self.matplotlib_colors['red'], self.matplotlib_colors['blue'], 
            self.matplotlib_colors['green'], self.matplotlib_colors['orange'],
            self.matplotlib_colors['purple'], self.matplotlib_colors['brown'],
            self.matplotlib_colors['pink'], self.matplotlib_colors['olive'],
            self.matplotlib_colors['cyan'], self.matplotlib_colors['magenta'],
            self.matplotlib_colors['lime'], self.matplotlib_colors['indigo'],
            self.matplotlib_colors['teal'], self.matplotlib_colors['maroon'],
            self.matplotlib_colors['navy'], self.matplotlib_colors['darkgreen'],
            self.matplotlib_colors['darkorange'], self.matplotlib_colors['darkred'],
            self.matplotlib_colors['darkblue'], self.matplotlib_colors['darkmagenta'],
            self.matplotlib_colors['forestgreen'], self.matplotlib_colors['firebrick'],
            self.matplotlib_colors['royalblue'], self.matplotlib_colors['seagreen'],
            self.matplotlib_colors['chocolate'], self.matplotlib_colors['crimson'],
            self.matplotlib_colors['steelblue'], self.matplotlib_colors['goldenrod'],
            self.matplotlib_colors['mediumblue'], self.matplotlib_colors['orangered']
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
                # Apply color theme
                dpg.bind_item_theme(f"series_{set_id}", f"theme_{set_id}")
    
    def create_themes(self):
        """Create color themes for different data sets"""
        # Create themes for all available plot colors
        for i, color in enumerate(self.plot_colors):
            with dpg.theme(tag=f"theme_{i}"):
                with dpg.theme_component(dpg.mvLineSeries):
                    dpg.add_theme_color(dpg.mvPlotCol_Line, color, category=dpg.mvThemeCat_Plots)
        
        # Create additional themes for all matplotlib colors (in case user wants more)
        for i, (name, color) in enumerate(self.matplotlib_colors.items()):
            theme_id = f"theme_mpl_{name}"
            with dpg.theme(tag=theme_id):
                with dpg.theme_component(dpg.mvLineSeries):
                    dpg.add_theme_color(dpg.mvPlotCol_Line, color, category=dpg.mvThemeCat_Plots)
    
    def load_data_callback(self, sender, app_data):
        """Callback for loading data from file"""
        try:
            file_path = app_data['file_path_name']
            x_data = []
            y_data = []
            
            # Try different parsing methods
            try:
                # Method 1: Try numpy loadtxt first
                data = np.loadtxt(file_path, comments=["@", "#"])
                if data.ndim == 1:
                    # Single row of data
                    if len(data) >= 2:
                        x_data = [data[0]]
                        y_data = [data[1]]
                else:
                    # Multiple rows
                    if data.shape[1] >= 2:
                        x_data = data[:, 0].tolist()
                        y_data = data[:, 1].tolist()
            except:
                # Method 2: Manual line-by-line parsing
                with open(file_path, 'r') as file:
                    for line in file:
                        line = line.strip()
                        if line and not line.startswith('@') and not line.startswith('#'):
                            try:
                                # Split by whitespace or comma
                                parts = line.replace(',', ' ').split()
                                if len(parts) >= 2:
                                    x_val = float(parts[0])
                                    y_val = float(parts[1])
                                    x_data.append(x_val)
                                    y_data.append(y_val)
                            except ValueError:
                                continue

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
    
    def on_data_selection(self, sender, selection):
        """Handle data set selection"""
        if selection >= 0 and selection < len(self.data_sets):
            self.current_set = selection
    
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
            # Delete the series from plot
            if dpg.does_item_exist(f"series_{selection}"):
                dpg.delete_item(f"series_{selection}")
            
            del self.data_sets[selection]
            
            # Reorganize data sets with new sequential IDs
            new_data_sets = {}
            for i, (old_id, data) in enumerate(sorted(self.data_sets.items())):
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
        
        # Create a light theme for the entire application
        with dpg.theme(tag="global_theme"):
            with dpg.theme_component(dpg.mvAll):
                dpg.add_theme_color(dpg.mvThemeCol_WindowBg, [240, 240, 240, 255])
                dpg.add_theme_color(dpg.mvThemeCol_ChildBg, [250, 250, 250, 255])
                dpg.add_theme_color(dpg.mvThemeCol_PopupBg, [240, 240, 240, 255])
                dpg.add_theme_color(dpg.mvThemeCol_Text, [0, 0, 0, 255])
                dpg.add_theme_color(dpg.mvThemeCol_Button, [200, 200, 200, 255])
                dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [150, 150, 150, 255])
                dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [100, 100, 100, 255])
                dpg.add_theme_color(dpg.mvThemeCol_Header, [180, 180, 180, 255])
                dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [150, 150, 150, 255])
                dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, [120, 120, 120, 255])
        
        # File dialogs
        with dpg.file_dialog(directory_selector=False, show=False, callback=self.load_data_callback, 
                           tag="file_dialog_load", width=800, height=400):
            dpg.add_file_extension("", color=[150, 255, 150, 255])  # Default (any file)
            dpg.add_file_extension(".*", color=[150, 255, 150, 255])
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
                
                # Left panel - Controls (reduced width)
                with dpg.child_window(width=300, height=750):
                    dpg.add_text("Data Sets")
                    dpg.add_listbox([], tag="data_listbox", width=280, num_items=8,
                                  callback=self.on_data_selection)
                    
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Toggle", callback=self.toggle_data_visibility, width=90)
                        dpg.add_button(label="Delete", callback=self.delete_data_set, width=90)
                    
                    dpg.add_separator()
                    
                    dpg.add_text("Plot Controls")
                    dpg.add_button(label="Auto Scale", callback=self.auto_scale, width=280)
                    
                    dpg.add_separator()
                    
                    dpg.add_text("Quick Function Generator")
                    dpg.add_input_text(label="f(x)", tag="function_input", 
                                      default_value="sin(x)", width=200)
                    
                    with dpg.group(horizontal=True):
                        dpg.add_input_double(label="Min", tag="x_min_input", 
                                           default_value=-10.0, width=90, format="%.1f")
                        dpg.add_input_double(label="Max", tag="x_max_input", 
                                           default_value=10.0, width=90, format="%.1f")
                    
                    dpg.add_input_int(label="Points", tag="n_points_input", 
                                    default_value=100, width=200)
                    dpg.add_button(label="Generate", callback=self.generate_function_data, width=280)
                
                # Right panel - Plot (expanded)
                with dpg.group():
                    with dpg.plot(label="Plot", height=750, width=850, tag="main_plot"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="X", tag="x_axis")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Y", tag="y_axis")
            
            # Status bar
            dpg.add_separator()
            dpg.add_text("Ready", tag="status_text")
        
        # Function generator window (unchanged)
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
        
        # Apply global theme
        dpg.bind_theme("global_theme")
        
        dpg.create_viewport(title="XMGrace-style Plotter", width=1200, height=850)
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
