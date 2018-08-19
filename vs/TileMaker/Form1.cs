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

namespace TileMaker
{
    public partial class Form1 : Form
    {
        [DllImport("tile.dll")]
        public static extern IntPtr osgb23dtile_path(string in_path, string out_path, IntPtr box, ref int len, int max_lvl);

        [DllImport("tile.dll")]
        public static extern void free_buffer(IntPtr buffer);

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
                            string tile_json = Marshal.PtrToStringAnsi(json);
                            free_buffer(json);
                            for(int i = 0; i < 3; i++)
                                  root_box[i] = box[i] > root_box[i] ? box[i] : root_box[i];
                            for (int i = 3; i < 6; i++)
                                root_box[i] = box[i] < root_box[i] ? box[i] : root_box[i];
                        }
                    }
                }
                catch(Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
            // 合并外包矩形，合并输出 json
            string tileset_json = "";
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
