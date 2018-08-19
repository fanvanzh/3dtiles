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
            foreach(DirectoryInfo f in DirSub)
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
                            int a = 10;
                        }
                    }
                }
                catch(Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            MessageBox.Show("not support now!");
        }
    }
}
