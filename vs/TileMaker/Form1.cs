using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;

using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace TileMaker
{
    public partial class Form1 : Form
    {
        public class Asset
        {
            public string version;
            public string gltfUpAxis;
        };
        public class Root
        {
            public struct BoundingVolume
            {
                public double[] box;
            }
            public struct Tile {
                public struct Content {
                    public string url;
                }
                public BoundingVolume boundingVolume;
                public double geometricError;
                public Content content;
            }
            public BoundingVolume boundingVolume;
            public double geometricError;
            public double[] transform;
            public List<Tile> children;
        };
        public class TileSet
        {
            public Asset asset;
            public double geometricError;
            public Root root;
        };

        [DllImport("tile.dll")]
        public static extern IntPtr osgb23dtile_path(string in_path, string out_path, IntPtr box, ref int len, int max_lvl);

        [DllImport("tile.dll")]
        public static extern void free_buffer(IntPtr buffer);

        [DllImport("tile.dll")]
        public static extern void transform_c(double center_x, double center_y, double height_min, IntPtr ptr);
        

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void button2_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog dialog = new FolderBrowserDialog();
            dialog.Description = "请选择OSGB数据文件夹";
            if (dialog.ShowDialog() == DialogResult.OK)
            {
                if (string.IsNullOrEmpty(dialog.SelectedPath))
                {
                    MessageBox.Show(this, "文件夹路径不能为空", "提示");
                    return;
                }
                string input_path = dialog.SelectedPath;
                input_path += "\\Data";
                if (!Directory.Exists(input_path)) {
                    MessageBox.Show(string.Format("请确认路径[{}]是否存在",input_path));
                } else {
                    input_osg.Text = dialog.SelectedPath;
                }
            }
        }

        private void button3_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog dialog = new FolderBrowserDialog();
            dialog.Description = "请选择OSGB数据文件夹";
            if (dialog.ShowDialog() == DialogResult.OK)
            {
                if (string.IsNullOrEmpty(dialog.SelectedPath))
                {
                    MessageBox.Show(this, "文件夹路径不能为空", "提示");
                    return;
                }
                string output_path = dialog.SelectedPath;
                output_osg.Text = output_path;
            }
        }

        private void button4_Click(object sender, EventArgs e)
        {
            if (output_osg.Text.Trim() == "" || input_osg.Text.Trim() == "")
            {
                MessageBox.Show("目录不能为空");
                return;
            }
            string out_path = output_osg.Text;
            if (!Directory.Exists(out_path))
            {
                try
                {
                    Directory.CreateDirectory(out_path);
                }
                catch ( Exception ex )
                {
                    MessageBox.Show(ex.Message);
                    return;
                }
            }
            // convert path A to path B
            convert_osgb(input_osg.Text.Trim(), output_osg.Text.Trim());
        }

        private void convert_osgb(string osgb_dir, string tile_dir)
        {
            string data_dir = osgb_dir;
            data_dir += "/Data";
            if (!Directory.Exists(data_dir))
            {
                MessageBox.Show(string.Format("未找到目录:{}", data_dir));
                return;
            }
            DirectoryInfo Dir = new DirectoryInfo(data_dir);
            DirectoryInfo[] DirSub = Dir.GetDirectories();

            double[] root_box = new double[] { -1.0E+38, -1.0E+38, -1.0E+38, 1.0E+38, 1.0E+38, 1.0E+38 };
            List<string> subtile_json = new List<string>();
            foreach (DirectoryInfo f in DirSub)
            {
                string osb_sub = f.FullName;
                osb_sub += string.Format("\\{0}.osgb", f.Name);
                if (!File.Exists(osb_sub)) {
                    MessageBox.Show(string.Format("无法找到文件:{}", osb_sub));
                    continue;
                }
                string out_path = tile_dir + "/Data/" + f.Name;
                Directory.CreateDirectory(out_path);
                try
                {
                    // call c
                    double[] box = new double[6];
                    unsafe
                    {
                        fixed (double* ptr = box)
                        {
                            int buf_len = 0;
                            IntPtr json = osgb23dtile_path(osb_sub, out_path, (IntPtr)ptr, ref buf_len, 18);
                            subtile_json.Add(Marshal.PtrToStringAnsi(json));
                            free_buffer(json);
                            for (int i = 0; i < 3; i++)
                                root_box[i] = box[i] > root_box[i] ? box[i] : root_box[i];
                            for (int i = 3; i < 6; i++)
                                root_box[i] = box[i] < root_box[i] ? box[i] : root_box[i];
                        }
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
            // 合并外包矩形，合并输出 json
            string tileset = @"{
                ""asset"": {
                ""gltfUpAxis"":""Y"",
                ""version"": ""0.0""
                },
                ""geometricError"": 2000,
                ""root"": {
                ""boundingVolume"": {
                    ""box"": [
                    0, 0, 0,
                    0, 0, 0,
                    0, 0, 0,
                    0, 0, 0
                    ]
                },
                ""geometricError"": 2000,
                ""transform"": [
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0
                ]
                }
            }";
            TileSet tilejson = JsonConvert.DeserializeObject<TileSet>(tileset);
            var bbox = make_tile_box(root_box);
            for(int i = 0; i < bbox.Count; i++)
            {
                tilejson.root.boundingVolume.box[i] = bbox[i];
            }
            // write the transform
            double[] root_transform = new double[16];
            unsafe
            {
                fixed(double* ptr = root_transform)
                {
                    transform_c(120, 30, 0, (IntPtr)ptr);
                }
            }
            //JArray transform = (JArray)tilejson["root"]["transform"];
            for(int i = 0; i < 16; i++)
            {
                tilejson.root.transform[i] = root_transform[i];
            }
            // add children
            if(tilejson.root.children == null)
            {
                tilejson.root.children = new List<Root.Tile>();
            }
            tilejson.root.children.Clear();
            foreach (var tile in subtile_json)
            {
                Root.Tile tileObject = JsonConvert.DeserializeObject<Root.Tile>(tile);
                tileObject.content.url = tileObject.content.url.Replace("./","./Data/").Replace(".b3dm", "/tileset.json");
                tilejson.root.children.Add(tileObject);

                string subtile_string = @"{
                ""asset"": {
                ""gltfUpAxis"":""Y"",
                ""version"": ""0.0""
                },
                ""root"": ";
                subtile_string += tile;
                subtile_string += "}";
                string tile_path = tileObject.content.url.Replace("./", "/");
                JObject obj = (JObject)JsonConvert.DeserializeObject(subtile_string);
                string format_string = JsonConvert.SerializeObject(obj, Formatting.Indented);
                File.WriteAllText(tile_dir + tile_path, format_string, Encoding.UTF8);
            }
            // write root
            string out_string = JsonConvert.SerializeObject(tilejson, Formatting.Indented);
            File.WriteAllText(tile_dir + "/tileset.json", out_string, Encoding.UTF8);
            MessageBox.Show("转换完成！");
        }

        private List<double> make_tile_box(double[] box)
        {
            List<double> tile_box = new List<double>();
            tile_box.Add((box[0] + box[3]) / 2.0);
            tile_box.Add((box[1] + box[4]) / 2.0);
            tile_box.Add((box[2] + box[5]) / 2.0);

            tile_box.Add((box[3] - box[0]) / 2.0);
            tile_box.Add(0.0);
            tile_box.Add(0.0);

            tile_box.Add(0.0);
            tile_box.Add((box[4] - box[1]) / 2.0);
            tile_box.Add(0.0);

            tile_box.Add(0.0);
            tile_box.Add((box[5] - box[2]) / 2.0);
            tile_box.Add(0.0);
            return tile_box;
        }

        private void button1_Click(object sender, EventArgs e)
        {
            MessageBox.Show("not support now!");
        }
    }
}
