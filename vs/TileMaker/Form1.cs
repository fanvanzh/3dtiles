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

namespace TileMaker
{
    public partial class Form1 : Form
    {
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
                //input_osg.Text = 
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
        }
    }
}
